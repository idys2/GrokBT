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

    // forward declare
    class Message;
    struct Buffer;  

    // message pack() -> buffer
    // buffer unpack() -> message
    struct Buffer { 
        uint32_t length;
        uint32_t bytes_read;
        std::unique_ptr<uint8_t[]> ptr;
        Buffer(uint32_t length);

        Message unpack();
    };

    class Message {
        
    };

    class Handshake : public Message {

    };


}

#endif