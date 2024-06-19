#ifndef MESSAGE_H
#define MESSAGE_H

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

        Buffer(uint32_t length); // For a nonhandshake message.
        Buffer(uint8_t pstrlen); // For a handshake message. The length of the buffer can be identified by the pstr length.
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
        uint32_t total_length;

    public:
        Message() {}
        virtual Buffer *pack() { return nullptr; }
        virtual void unpack(Buffer *buff) {}
    };

    // The handshake message as specified by the bittorrent protocol.
    class Handshake : public Message
    {

    private:
        uint8_t pstrlen;
        std::string pstr;
        uint8_t reserved[8];
        std::string info_hash;
        std::string peer_id;

    public:
        Handshake(uint8_t pstrlen, std::string pstr, std::string info_hash, std::string peer_id);
        Handshake();

        // must be freed by the caller!
        Buffer *pack();
        void unpack(Buffer *buff);

        std::string get_peer_id();
        std::string get_info_hash();
        std::string to_string();
    };

}

#endif