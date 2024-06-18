#ifndef MESSAGE_H
#define MESSAGE_H

#include <string>
#include <assert.h>
#include <memory>

#include "hash.h"
#include "tcp_utils.h"

namespace Messages {
    // message IDs
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
    const uint8_t HANDSHAKE_ID = 10;    // this isn't part of the bittorrent protocol, but is used internally

    struct Buffer {
        uint32_t total_length;          // the total length of this buffer, length of message + sizeof(uint32_t)
        uint32_t bytes_read;            // the number of bytes that have been read
        std::unique_ptr<uint8_t[]> ptr;

        Buffer(uint32_t length);        // For a nonhandshake message
        Buffer(uint8_t pstrlen);        // For a handshake message
    };
    
    class Message {
        protected:
            uint32_t total_length;  
        public:
            Message();
            virtual Buffer *pack();
            virtual void unpack(Buffer *buff);
    };

    class Handshake : public Message {

        private: 
            uint8_t pstrlen;
            std::string pstr;
            uint8_t reserved[8];
            std::string info_hash; 
            std::string peer_id;

        public: 
            Handshake(uint8_t pstrlen, std::string pstr, std::string info_hash, std::string peer_id);

            Buffer *pack();
            void unpack(Buffer *buff);

    };


}

#endif