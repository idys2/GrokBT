#ifndef MESSAGE_H
#define MESSAGE_H

#include <string>
#include <assert.h>

#include "hash.h"
#include "tcp_utils.h"

namespace Message {
    // message IDs
    const uint8_t CHOKE_ID = 0;
    const uint8_t UNCHOKE_ID = 1;
    const uint8_t INTERESTED_ID = 2;
    const uint8_t UNINTERESTED_ID = 3;
    const uint8_t HAVE_ID = 4;
    const uint8_t BITFIELD_ID = 5;
    const uint8_t REQUEST_ID = 6    ;
    const uint8_t PIECE_ID = 7;     // used on seeder side
    const uint8_t CANCEL_ID = 8;    // only used if implementing endgame
    const uint8_t PORT_ID = 9;      // only used if implementing a DHT tracker

    class Message {

        public: 
            // STORED IN HOST BYTE ORDER, REMEMBER TO CONVERT TO NETWORK BYTE BEFORE SENDING
            uint32_t len;               // the length of this message, not including this field

            virtual void pack(char *buffer) {}    // pack this message into the arg char* buffer. Should not try to pack len header.
            virtual void unpack(uint8_t *buffer, uint32_t l) {}  // unpack arg buffer into this message. Should not try to unpack len headers for any messsage, this is passed in

            // send this message over the socket. Create a buffer 
            // that is large enough for a 4 byte len header, then pack len bytes 
            // for the message
            virtual void send_msg(int sock);

            Message() {}
    };

}

#endif