#ifndef MESSAGE_H
#define MESSAGE_H

#include <cstring>
#include <string>
#include <assert.h>
#include <memory>

#include "hash.h"
#include "tcp_utils.h"

namespace Messages
{
    // message IDs assigned by bittorrent protocol
    const uint8_t CHOKE_ID = 0;
    const uint8_t UNCHOKE_ID = 1;
    const uint8_t INTERESTED_ID = 2;
    const uint8_t UNINTERESTED_ID = 3;
    const uint8_t HAVE_ID = 4;
    const uint8_t BITFIELD_ID = 5;
    const uint8_t REQUEST_ID = 6;
    const uint8_t PIECE_ID = 7;
    const uint8_t CANCEL_ID = 8;
    const uint8_t PORT_ID = 9;

    // Holds data for peers that are recv'd on the wire. Because we use nonblocking sockets,
    // we need buffers for each peer that will be held as long as a message is not read completely in a single recv.
    // This basically wraps a pointer to raw bytes.
    struct Buffer
    {
        uint32_t total_length;          // the total length of this buffer
        uint32_t bytes_read;            // the number of bytes that have been read
        std::unique_ptr<uint8_t[]> ptr; // the pointer to the buffer that we are recv'ing into.

        Buffer(uint32_t length) // For a nonhandshake message.
        {
            bytes_read = 0;
            total_length = length + sizeof(length); // include the size of the length field in the buffer
            ptr = std::make_unique<uint8_t[]>(total_length);
        }
        Buffer(uint8_t pstrlen) // For a handshake message. The length of the buffer can be identified by the pstr length.
        {
            bytes_read = 0;
            total_length = 49 + pstrlen;
            ptr = std::make_unique<uint8_t[]>(total_length);
        }
    };

    // An abstract class that will be inherited for each different kind of message.
    // -When the client needs to send a particular message, it will call
    // pack() to receive a pointer to a buffer struct that has been formatted correctly
    // according to the bittorrent protocol for that message type. This buffer can then be sent over the wire.
    // -When the client wants to recv a message (note that the type of message can always be identified given the state
    // of the torrent), the client calls unpack(Buffer *b) to fill out all message fields according to the data in the buffer

    // Note: the pointer returned by the pack() function must be freed by the caller.
    class Message
    {
    protected:
        uint32_t total_length; // the total length of the message, *including* the length field header

    public:
        Message() {}
        Message(Buffer *buff) // unpack a buffer into this message
        {
            total_length = buff->total_length;
        }
        virtual void pack(Buffer *buff) {} // pack this message into the buffer
    };

    // Handshake message as specified by the bittorrent protocol.
    class Handshake : public Message
    {

    private:
        uint8_t pstrlen;
        std::string pstr;
        uint8_t reserved[8];
        std::string info_hash;
        std::string peer_id;

    public:
        Handshake(uint8_t pstrlen, std::string pstr, std::string info_hash, std::string peer_id)
        {
            this->pstrlen = pstrlen;
            this->pstr = pstr;
            this->info_hash = info_hash;
            this->peer_id = peer_id;
            memset(&reserved[0], 0, 8);
            this->total_length = 49 + pstrlen;
        }

        Handshake(Buffer *buff); // unpack a buffer into this message

        void pack(Buffer *buff); // pack this handshake into argument buffer

        std::string get_peer_id()
        {
            return peer_id;
        }
        std::string get_info_hash()
        {
            return info_hash;
        }
        uint8_t get_pstrlen()
        {
            return pstrlen;
        }
        std::string to_string()
        {
            return "Peer id: " + peer_id + "\n" + "Info hash: " + info_hash + "\n" + "pstr: " + pstr + "\n";
        }
    };

    // The keep alive as specified by bittorrent protocol.
    class KeepAlive : public Message
    {
    private:
        uint32_t len;

    public:
        KeepAlive()
        {
            len = 0;
        }
        KeepAlive(Buffer *buff)
        {
            memcpy(&len, buff->ptr.get(), sizeof(len));
        }
        void pack(Buffer *buff)
        {
            memcpy(buff->ptr.get(), &len, sizeof(len));
        }
    };

    // A base class for fixed length messages which may or may not have a payload
    // This includes choke, unchoke, interested, notinterested, request, have
    class FixedLengthMessage : public Message
    {
    protected:
        uint32_t len;
        uint8_t id;

    public:
        FixedLengthMessage(uint32_t len, uint8_t id)
        {
            this->len = len;
            this->id = id;
        }
        FixedLengthMessage(Buffer *buff)
        {
            int idx = 0;

            // read the len field
            memcpy(&len, buff->ptr.get() + idx, sizeof(len));
            idx += sizeof(len);

            // read the id  field
            memcpy(&id, buff->ptr.get() + idx, sizeof(id));
        }
        void pack(Buffer *buff)
        {
            int idx = 0;

            // pack the len first
            memcpy(buff->ptr.get() + idx, &len, sizeof(len));
            idx += sizeof(len);

            // pack the id
            memcpy(buff->ptr.get() + idx, &id, sizeof(id));
        }
    };

    // A choke message in the bittorrent protocol
    class Choke : public FixedLengthMessage
    {
    public:
        Choke(uint32_t len_field, uint8_t id_field) : FixedLengthMessage(len_field, id_field) {}
        Choke(Buffer *buff) : FixedLengthMessage(buff) {}
    };

    // An unchoke message in the bittorrent protocol
    class Unchoke : public FixedLengthMessage
    {
    public:
        Unchoke(uint32_t len_field, uint8_t id_field) : FixedLengthMessage(len_field, id_field) {}
        Unchoke(Buffer *buff) : FixedLengthMessage(buff) {}
    };

    // An interested message in the bittorrent protocol
    class Interested : public FixedLengthMessage
    {
    public:
        Interested(uint32_t len_field, uint8_t id_field) : FixedLengthMessage(len_field, id_field) {}
        Interested(Buffer *buff) : FixedLengthMessage(buff) {}
    };

    // A notinterested message in the bittorrent protocol
    class NotInterested : public FixedLengthMessage
    {
    public:
        NotInterested(uint32_t len_field, uint8_t id_field) : FixedLengthMessage(len_field, id_field) {}
        NotInterested(Buffer *buff) : FixedLengthMessage(buff) {}
    };

    // A have message in the bittorrent protocol
    class Have : public FixedLengthMessage
    {
    private:
        uint32_t piece_index; // the piece index that we are indicating we have

    public:
        Have(uint32_t len_field, uint8_t id_field, uint32_t piece_index) : FixedLengthMessage(len_field, id_field)
        {
            this->piece_index = piece_index;
        }
        Have(Buffer *buff) : FixedLengthMessage(buff)
        {
            // parent constructor has already read len and id fields
            memcpy(&piece_index, buff->ptr.get() + sizeof(len) + sizeof(id), sizeof(piece_index));
        }
        Buffer *pack();
    };

    // A request message in the bittorrent protocol
    class Request : public FixedLengthMessage
    {
    private:
        uint32_t index;  // piece index that is being requested
        uint32_t begin;  // byte offset that the requested block is for
        uint32_t length; // how long the block being requested is
    public:
        Request(uint32_t len_field, uint8_t id_field, uint32_t index, uint32_t begin, uint32_t length);
        Request(Buffer *buff);
    };

}

#endif