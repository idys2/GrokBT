#ifndef MESSAGE_H
#define MESSAGE_H

#include <cstring>
#include <string>
#include <assert.h>
#include <memory>

#include "hash.h"
#include "file.h"
#include "tcp_utils.h"

namespace Messages
{
    // This file mostly refers to: https://wiki.theory.org/BitTorrentSpecification#Tracker_HTTP.2FHTTPS_Protocol
    // under peer wire protocol.

    // message IDs assigned by bittorrent protocol
    const uint8_t CHOKE_ID = 0;
    const uint8_t UNCHOKE_ID = 1;
    const uint8_t INTERESTED_ID = 2;
    const uint8_t NOTINTERESTED_ID = 3;
    const uint8_t HAVE_ID = 4;
    const uint8_t BITFIELD_ID = 5;
    const uint8_t REQUEST_ID = 6;
    const uint8_t PIECE_ID = 7;
    const uint8_t CANCEL_ID = 8;
    const uint8_t PORT_ID = 9;
    const int HAVE_LENGTH = 5;
    const int REQUEST_LENGTH = 13;

    // Holds data that is recv'd on the wire. Because we use nonblocking sockets,
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
            total_length = length; // include the size of the length field in the buffer
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
    // - When the client needs to send a particular message, it will call
    // pack() to receive a pointer to a buffer struct that has been formatted correctly
    // according to the bittorrent protocol for that message type. This buffer can then be sent over the wire.
    // - When the client wants to recv a message (note that the type of message can always be identified given the state
    // of the torrent), the client calls Constructor(Buffer *b) to fill out all message fields according to the data in the buffer

    class Message
    {
    protected:
        uint32_t total_length; // the total length of the message, *including* the length field header

    public:
        Message() {}
        Message(Buffer *buff) // unpack a buffer into this message
        {
            this->total_length = buff->total_length;
        }
        virtual Buffer *pack() { return nullptr; } // pack this message and return a pointer to a new buffer
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

        Handshake(Buffer *buff) : Message(buff)
        {
            int idx = 0;
            // read the pstrlen field
            memcpy(&pstrlen, buff->ptr.get() + idx, sizeof(pstrlen));
            idx += sizeof(pstrlen);

            // read the pstr field
            pstr = std::string((const char *)(buff->ptr.get() + idx), pstrlen);
            idx += pstrlen;

            // read the reserved field
            memcpy(&reserved[0], buff->ptr.get() + idx, 8);
            idx += 8;

            // read the info hash field
            info_hash = std::string((const char *)(buff->ptr.get() + idx), 20);
            idx += 20;

            // read the peer_id field
            peer_id = std::string((const char *)(buff->ptr.get() + idx), 20);
            idx += 20;
        }

        Buffer *pack()
        {
            Buffer *buffer = new Buffer(total_length);
            // for advancing the pointer when we write to the buffer
            int idx = 0;

            // pack the pstrlen first
            memcpy(buffer->ptr.get() + idx, &pstrlen, sizeof(pstrlen));
            idx += sizeof(pstrlen);

            // pack the pstr
            memcpy(buffer->ptr.get() + idx, pstr.c_str(), pstrlen);
            idx += pstrlen;

            // pack the reserved bytes
            memcpy(buffer->ptr.get() + idx, &reserved[0], 8);
            idx += 8;

            // pack the info hash
            memcpy(buffer->ptr.get() + idx, info_hash.c_str(), 20);
            idx += 20;

            // pack the peer id
            memcpy(buffer->ptr.get() + idx, peer_id.c_str(), 20);

            return buffer;
        }

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

        int get_total_len()
        {
            return total_length;
        }
    };

    // ----------------------FIXED LENGTH MESSAGES -----------------------------
    // A base class for messages which may or may not have a payload
    class BaseMessage : public Message
    {
    protected:
        uint32_t len;
        uint8_t id;

    public:
        BaseMessage(uint32_t len, uint8_t id)
        {
            this->len = len;
            this->id = id;
            total_length = sizeof(len) + sizeof(id);
        }
        BaseMessage(Buffer *buff) : Message(buff)
        {
            int idx = 0;

            // read the len field
            // already converted from big endian!
            memcpy(&len, buff->ptr.get() + idx, sizeof(len));
            idx += sizeof(len);

            // read the id  field
            memcpy(&id, buff->ptr.get() + idx, sizeof(id));
        }
        Buffer *pack()
        {
            Buffer *buff = new Buffer(total_length);
            int idx = 0;

            // pack the len first
            uint32_t big_endian_len = htonl(len);
            memcpy(buff->ptr.get() + idx, &big_endian_len, sizeof(big_endian_len));
            idx += sizeof(big_endian_len);

            // pack the id
            memcpy(buff->ptr.get() + idx, &id, sizeof(id));

            return buff;
        }
    };

    // A choke message in the bittorrent protocol
    class Choke : public BaseMessage
    {
    public:
        Choke() : BaseMessage(sizeof(CHOKE_ID), CHOKE_ID) {}
        Choke(Buffer *buff) : BaseMessage(buff) {}
    };

    // An unchoke message in the bittorrent protocol
    class Unchoke : public BaseMessage
    {
    public:
        Unchoke() : BaseMessage(sizeof(UNCHOKE_ID), UNCHOKE_ID) {}
        Unchoke(Buffer *buff) : BaseMessage(buff) {}
    };

    // An interested message in the bittorrent protocol
    class Interested : public BaseMessage
    {
    public:
        Interested() : BaseMessage(sizeof(INTERESTED_ID), INTERESTED_ID) {}
        Interested(Buffer *buff) : BaseMessage(buff) {}
    };

    // A notinterested message in the bittorrent protocol
    class NotInterested : public BaseMessage
    {
    public:
        NotInterested() : BaseMessage(sizeof(NOTINTERESTED_ID), NOTINTERESTED_ID) {}
        NotInterested(Buffer *buff) : BaseMessage(buff) {}
    };

    // A have message in the bittorrent protocol
    class Have : public BaseMessage
    {
    private:
        uint32_t piece_index; // the piece index that we are indicating we have

    public:
        Have(uint32_t piece_index) : BaseMessage(HAVE_LENGTH, HAVE_ID)
        {
            this->piece_index = piece_index;
            total_length = HAVE_LENGTH + sizeof(len);
        }
        Have(Buffer *buff) : BaseMessage(buff)
        {
            int idx = sizeof(len) + sizeof(id);
            memcpy(&piece_index, buff->ptr.get() + idx, sizeof(piece_index));
            piece_index = ntohl(piece_index);
        }
        Buffer *pack()
        {
            Buffer *buff = new Buffer(total_length);
            int idx = 0;

            // pack the len first
            uint32_t big_endian_len = htonl(len);
            memcpy(buff->ptr.get() + idx, &big_endian_len, sizeof(big_endian_len));
            idx += sizeof(big_endian_len);

            // pack the id
            memcpy(buff->ptr.get() + idx, &id, sizeof(id));
            idx += sizeof(id);

            uint32_t big_endian_piece_index = htonl(piece_index);
            memcpy(buff->ptr.get() + idx, &big_endian_piece_index, sizeof(big_endian_piece_index));
            idx += sizeof(big_endian_piece_index);

            return buff;
        }
    };

    // A request message in the bittorrent protocol
    class Request : public BaseMessage
    {
    private:
        uint32_t index;  // piece index that is being requested
        uint32_t begin;  // byte offset that the requested block is for
        uint32_t length; // how long the block being requested is
    public:
        Request(uint32_t index, uint32_t begin, uint32_t length) : BaseMessage(REQUEST_LENGTH, REQUEST_ID)
        {
            this->index = index;
            this->begin = begin;
            this->length = length;

            total_length = REQUEST_LENGTH + sizeof(len);
        }

        Request(Buffer *buff) : BaseMessage(buff)
        {
            // we already read len, id in BaseMessage constructor
            int idx = sizeof(len) + sizeof(id);

            memcpy(&index, buff->ptr.get() + idx, sizeof(index));
            idx += sizeof(index);

            memcpy(&begin, buff->ptr.get() + idx, sizeof(begin));
            idx += sizeof(begin);

            memcpy(&length, buff->ptr.get() + idx, sizeof(length));
            idx += sizeof(length);

            index = ntohl(index);
            begin = ntohl(begin);
            length = ntohl(length);
        }

        Buffer *pack()
        {

            Buffer *buff = new Buffer(total_length);
            int idx = 0;

            uint32_t big_endian_len = htonl(len);
            memcpy(buff->ptr.get() + idx, &big_endian_len, sizeof(big_endian_len));
            idx += sizeof(big_endian_len);

            memcpy(buff->ptr.get() + idx, &id, sizeof(id));
            idx += sizeof(id);


            uint32_t big_endian_index = htonl(index);
            memcpy(buff->ptr.get() + idx, &big_endian_index, sizeof(big_endian_index));
            idx += sizeof(big_endian_index);

            uint32_t big_endian_begin = htonl(len);
            memcpy(buff->ptr.get() + idx, &big_endian_begin, sizeof(big_endian_begin));
            idx += sizeof(big_endian_begin);

            uint32_t big_endian_length = htonl(len);
            memcpy(buff->ptr.get() + idx, &big_endian_length, sizeof(big_endian_length));
            idx += sizeof(big_endian_length);
 
            return buff;
        }
    };

    // ----------------------VARIABLE LENGTH MESSAGES -----------------------------
    // A bitfield message, as specified by bittorrent protocol
    // Because bitfields are also implemented as an important struct with other
    // functionality, for bitfields we only need to translate between these structs
    // and buffers.

    // interpret data stored in the buffer as a bitfield message
    // returns a bitfield struct as defined in file.h
    File::BitField *parseBitField(Buffer *buff);

    // Pack a bitfield into a buffer
    Buffer bitFieldToBuffer(File::BitField bitfield);

}

#endif