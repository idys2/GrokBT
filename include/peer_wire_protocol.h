#ifndef _PEER_WIRE_PROTOCOL_H
#define _PEER_WIRE_PROTOCOL_H
#include "hash.h"
#include <string>

namespace PeerWireProtocol { 
    
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

    // TODO: get rid of new in messages
    // using smart pointers?
    class Message {

        public: 
            // STORED IN HOST BYTE ORDER, REMEMBER TO CONVERT TO NETWORK BYTE BEFORE SENDING
            uint32_t len;               // the length of this message, not including this field

            // these will possibly be overloaded in child classes

            virtual void pack(char *buffer) {}    // pack this message into the arg char* buffer. Should not try to pack len header.
            virtual void unpack(uint8_t *buffer, uint32_t l) {}  // unpack arg buffer into this message. Should not try to unpack len headers for any messsage, this is passed in

            // send this message over the socket. Create a buffer 
            // that is large enough for a 4 byte len header, then pack len bytes 
            // for the message
            virtual void send_msg(int sock) {     
                size_t total_len = len + sizeof(len);
                uint32_t mut_len = total_len;       // this is used because sendall will modify the variable
                uint32_t net_byte_len = htonl(len);

                char *buffer = new char[total_len];
                memcpy(buffer, &net_byte_len, sizeof(net_byte_len));  // write the len first

                pack(buffer + sizeof(len));         // pack everything after len
                assert(ProtocolUtils::sendall(sock, buffer, &mut_len) == 0);
                
                delete[] buffer;
            }

            Message() {}
    };

    class Handshake : public Message {
        
        std::string pstr = "BitTorrent protocol";        
        uint8_t pstrlen = static_cast<uint8_t>(pstr.length()); 
        uint8_t reserved[8] = {0}; // init to 0

        public: 
            
            std::string info_hash;  // from tracker requests
            std::string peer_id;    // from tracker requests
            
            Handshake() {
                len = 48 + 1 + pstrlen;
            }

            Handshake(std::string hash, std::string pid) : Message() { 
                info_hash = hash; 
                peer_id = pid; 
                len = 48 + 1 + pstrlen;
            }

            /*
            * Pack this handshake message payload into the buffer
            */
            void pack(char *buffer) override {
                assert(info_hash.length() == 20); 
                assert(peer_id.length() == 20);

                // write pstr
                memcpy(buffer, pstr.c_str(), pstrlen);
                buffer += pstrlen; 

                // write reserved bytes
                memcpy(buffer, &reserved[0], 8);
                buffer += 8;
                
                // write info hash
                memcpy(buffer, info_hash.c_str(), 20);        
                buffer += 20; 
                
                // write peer id
                memcpy(buffer, peer_id.c_str(), 20);
            }

            /*
            * Unpack a buffer into this Handshake message, using a received pstrlen. Overloads the 
            * the unpack from the parent class, because handshake messages don't have total length at front of message.
            */
            void unpack(uint8_t *buffer, uint32_t pstrlen) override {

                char info_hash_buffer[20]; 
                char peer_id_buffer[20];

                // value we recv for pstrlen
                char* pstr_buffer = new char[pstrlen];
                
                // read pstr
                memcpy(pstr_buffer, buffer, pstrlen);
                pstr.assign(pstr_buffer, pstrlen); 
                buffer += pstrlen;
                
                delete[] pstr_buffer;

                // read reserved
                memcpy(&reserved[0], buffer, 8); 
                buffer += 8; 

                // read info hash 
                memcpy(info_hash_buffer, buffer, 20);
                buffer += 20; 

                // read peer id
                memcpy(peer_id_buffer, buffer, 20);

                // convert to strings
                info_hash.assign(info_hash_buffer, 20);
                peer_id.assign(peer_id_buffer, 20);

                len = 48 + 1 + pstrlen;
            }

            /*
            * Send a handshake packet to sock. 
            */
            void send_msg(int sock) override {
                uint32_t mut_len = len;
                char *buffer = new char[len];

                // write pstrlen
                memcpy(buffer, &pstrlen, sizeof(pstrlen));
                pack(buffer + sizeof(pstrlen)); 

                assert(ProtocolUtils::sendall(sock, buffer, &mut_len) == 0);
                delete[] buffer;
            }
            
            /**
            * Receive handshake packet from sock.
            */
            void recv_msg(int sock) {

                // receive first byte of handshake to determine size of buffer (only variable length is pstrlen)
                uint8_t pstrlen;
                recv(sock, &pstrlen, sizeof(pstrlen), 0);
                //while (bytes_sent < sizeof(pstrlen)) bytes_sent += recv(sock, &pstrlen + bytes_sent, sizeof(pstrlen) - bytes_sent, 0);

                // receive the rest of the handshake
                // note that we already received 1 byte for the pstrlen
                size_t remaining = pstrlen + 49 - sizeof(pstrlen);
                uint8_t *buffer = new uint8_t[remaining];
                int retmungus = recv(sock, buffer, remaining, 0);

                while (retmungus < remaining) { 
                    
                    retmungus += recv(sock, buffer + retmungus, remaining - retmungus, 0);
                }
                
                //int retmungus = recv(sock, buffer, pstrlen + 49 - sizeof(pstrlen), MSG_WAITALL);
                //assert(retmungus == pstrlen + 49 - sizeof(pstrlen));
                
                unpack(buffer, pstrlen);
                
                delete[] buffer;
            }

            /*
            * Get the string representation
            */
            std::string str() {
                std::string out = "";

                out += "pstr: " + pstr + "\n"; 
                out += "pstrlen: " + std::to_string(pstrlen) + "\n";
                //out += "infohash: " + info_hash + "\n"; 
                out += "peerid: " + peer_id + "\n";
                return out; 
            }
    };

    class KeepAlive : Message {
        /**
         * KeepAlive to keep a connection to a peer alive if no other messages are being sent (generally 2 minutes).
        */
        public: 
            KeepAlive() : Message() {
                len = 0;
            }

            void pack(char *buffer) override {}
            void unpack(uint8_t *buffer, uint32_t l) override {} 

            void send_msg(int sock) {
                Message::send_msg(sock);
            }
    };

    class Choke : public Message {
        /**
         * Choke the peer. All messages received from the peer should be discarded after this message is sent.
        */

        public: 
            uint8_t id;

            Choke() {
                len = sizeof(id);
                id = CHOKE_ID;
            }

            void pack(char *buffer) override {
                memcpy(buffer, &id, sizeof(id)); 
            } 

            void unpack(uint8_t *buffer, uint32_t l) override {
                len = l;
                memcpy(&id, buffer, sizeof(id));
            } 

            void send_msg(int sock) {
                Message::send_msg(sock);
            } 

    };

    class Unchoke : public Message {
        /**
         * Unchoke the peer. Handle messages from the peer after this message is sent.
        */
        
        public:
            uint8_t id; 

            Unchoke() {
                len = sizeof(id);
                id = UNCHOKE_ID;
            }

            void pack(char *buffer) override {
                memcpy(buffer, &id, sizeof(id)); 
            } 

            void unpack(uint8_t *buffer, uint32_t l) override {
                len = l;
                memcpy(&id, buffer, sizeof(id));
            } 

            void send_msg(int sock) {
                Message::send_msg(sock);
            }

    };

    class Interested : public Message {
        /**
         * We are interested in the peer.
        */

        uint8_t id; 
        public:
            Interested(){
                len = sizeof(id);
                id = INTERESTED_ID;
            }

            void pack(char *buffer) override {
                memcpy(buffer, &id, sizeof(id)); 
            } 

            void unpack(uint8_t *buffer, uint32_t l) override {
                len = l;
                memcpy(&id, buffer, sizeof(id));
            } 
            void send_msg(int sock) {
                Message::send_msg(sock);
            } 
    };

    class NotInterested : Message {

        /**
         * We are uninterested in the peer.
        */

        public:
            uint8_t id; 

            NotInterested() {
                len = sizeof(id);
                id = UNINTERESTED_ID;
            }

            void pack(char *buffer) override {
                memcpy(buffer, &id, sizeof(id)); 
            } 

            void unpack(uint8_t *buffer, uint32_t l) override {
                len = l;
                memcpy(&id, buffer, sizeof(id));
            } 

            void send_msg(int sock) {
                Message::send_msg(sock);
            } 

    };

    class Have : Message {

        /**
        * We successfully downloaded and verified via the hash for a piece.
        */
        
        public:
            uint32_t piece_index;
            uint8_t id;

            Have(uint32_t pi) : Message() {
                len = sizeof(id) + sizeof(piece_index);
                id = HAVE_ID;
                piece_index = pi;
            }

            Have() {}

            void pack(char *buffer) override {

                // pack id
                memcpy(buffer, &id, sizeof(id)); 
                buffer += sizeof(id); 

                // pack piece index
                uint32_t net_piece_index = htonl(piece_index);
                memcpy(buffer, &net_piece_index, sizeof(piece_index)); 
            } 

            void unpack(uint8_t *buffer, uint32_t l) override {
                len = l;
                memcpy(&id, buffer, sizeof(id));
            } 

            void send_msg(int sock) {
                Message::send_msg(sock);
            } 

    };

    class Bitfield : public Message {

        public: 
            uint32_t bitfield_len;  // bitfield length in BYTES
            uint8_t id;
            std::vector<uint8_t> bitfield;

            Bitfield(uint32_t bf_len) : Message() {
                bitfield_len = bf_len;
                len = sizeof(id) + bitfield_len;  // len = 5
                id = BITFIELD_ID;
                bitfield = std::vector<uint8_t>();
            }

            void pack(char *buffer) override {

                // write id 
                memcpy(buffer, &id, sizeof(id)); 
                buffer += sizeof(id); 

                // write bitfield
                for (auto i = 0; i < bitfield_len; i++) {
                    buffer[i + 1] = bitfield.at(i);
                }
            } 

            void unpack(uint8_t *buffer, uint32_t l) override {
                len = l;
                bitfield_len = len - sizeof(id);

                // read id 
                memcpy(&id, buffer, sizeof(id));
                buffer += sizeof(id); 

                // read bitfield 
                for (auto i = 0; i < bitfield_len; i++) {
                    bitfield.push_back(buffer[i]);
                }
            } 

            /*void send_msg(int sock) {
                Message::send_msg(sock);
            } */

    };

    class Request : public Message {

        public: 
            uint32_t piece_index;
            uint32_t begin_index;
            uint32_t requested_length;
            uint8_t id; 

            Request(uint32_t pi, uint32_t bi, uint32_t r_len) : Message() {
                len = sizeof(id) + sizeof(uint32_t) * 3;   // len = 13
                id = REQUEST_ID;
                piece_index = pi;
                begin_index = bi;
                requested_length = r_len;
            }

            Request() {}

            void pack(char *buffer) override {
                
                uint32_t net_piece_index = htonl(piece_index);
                uint32_t net_begin_index = htonl(begin_index);
                uint32_t net_requested_length = htonl(requested_length);

                memcpy(buffer, &id, sizeof(id)); 
                buffer += sizeof(id); 

                memcpy(buffer, &net_piece_index, sizeof(piece_index)); 
                buffer += sizeof(piece_index); 

                memcpy(buffer, &net_begin_index, sizeof(begin_index)); 
                buffer += sizeof(begin_index); 

                memcpy(buffer, &net_requested_length, sizeof(requested_length)); 
                buffer += sizeof(requested_length); 
            }

            // TODO: convert these to host encoding
            /**
             * @param[in]   buffer  buffer of ID + payload (DOES NOT INCLUDE LEN)
             * @param[in]   l       length of the buffer
            */
            void unpack(uint8_t *buffer, uint32_t l) override {
                len = l;
                
                memcpy(&id, buffer, sizeof(id)); 
                buffer += sizeof(id); 

                memcpy(&piece_index, buffer, sizeof(piece_index)); 
                buffer += sizeof(piece_index); 

                memcpy(&begin_index, buffer, sizeof(begin_index)); 
                buffer += sizeof(begin_index); 

                memcpy(&requested_length, buffer, sizeof(requested_length)); 
                buffer += sizeof(requested_length); 

                piece_index = ntohl(piece_index);
                begin_index = ntohl(begin_index);
                requested_length = ntohl(requested_length);
            }

            /*void send_msg(int sock) {
                Message::send_msg(sock);
            } */
    };

    // use a class that strips down the message
    class Piece : public Message {
        public:
            uint32_t piece_len;
            uint32_t piece_index;
            uint32_t begin_index;
            std::vector<uint8_t> piece;
            uint8_t id = PIECE_ID;
            
            Piece() {}
            
            // read l bytes from the buffer
            void unpack(uint8_t* buffer, uint32_t l) {
                len = l;
                piece_len = len - sizeof(id) - sizeof(uint32_t) * 2;

                // read id 
                memcpy(&id, buffer, sizeof(id));
                buffer += sizeof(id); 

                // read piece index
                memcpy(&piece_index, buffer, sizeof(piece_index));
                buffer += sizeof(piece_index);

                // read begin index
                memcpy(&begin_index, buffer, sizeof(begin_index));
                buffer += sizeof(begin_index);

                for(int i = 0; i < piece_len; i++) {
                    piece.push_back(buffer[i]);
                }
                
                piece_index = ntohl(piece_index);
                begin_index = ntohl(begin_index);
            } 

            /*void pack(char *buffer) override {

                // write id 
                memcpy(buffer, &id, sizeof(id)); 
                buffer += sizeof(id); 

                // write piece index
                memcpy(buffer, &piece_index, sizeof(piece_index));
                buffer += sizeof(piece_index);

                // write begin index
                memcpy(buffer, &begin_index, sizeof(begin_index));
                buffer += sizeof(begin_index);

                // write piece
                memcpy(buffer, piece, piece_len);
            } */
    };

    
    // unused
    class Cancel : Message {
        uint32_t piece_index;
        uint32_t begin_index;
        uint32_t requested_length;
        uint8_t id; 
        Cancel(uint32_t pi, uint32_t bi, uint32_t c_len) : Message() {
            len = sizeof(id) + sizeof(uint32_t) * 3;   // len = 13
            id = CANCEL_ID;
            piece_index = pi;
            begin_index = bi;
            requested_length = c_len;
        }
    };


    // stripped down Req message
    /*class RequestMsg {

        public: 
            uint32_t piece_index;
            uint32_t begin_index;
            uint32_t requested_length;
            uint32_t len;
            uint8_t id; 

            RequestMsg(uint32_t pi, uint32_t bi, uint32_t r_len) {
                len = sizeof(id) + sizeof(uint32_t) * 3;   // len = 13
                id = REQUEST_ID;
                piece_index = pi;
                begin_index = bi;
                requested_length = r_len;
            }

            void pack(char *buffer) {
                uint32_t net_len = htonl(len);
                uint32_t net_piece_index = htonl(piece_index);
                uint32_t net_begin_index = htonl(begin_index);
                uint32_t net_requested_length = htonl(requested_length);

                memcpy(buffer, &net_len, sizeof(net_len));
                buffer += sizeof(net_len);

                memcpy(buffer, &id, sizeof(id)); 
                buffer += sizeof(id); 

                memcpy(buffer, &net_piece_index, sizeof(piece_index)); 
                buffer += sizeof(piece_index); 

                memcpy(buffer, &net_begin_index, sizeof(begin_index)); 
                buffer += sizeof(begin_index); 

                memcpy(buffer, &net_requested_length, sizeof(requested_length)); 
                buffer += sizeof(requested_length); 
            } 

            void send_msg(int sock) {
                char *buffer = new char[len + sizeof(len)];
                pack(buffer);

                uint32_t mut_len = len + sizeof(len);
                assert(ProtocolUtils::sendall(sock, buffer, &mut_len) == 0);
            } 
    };*/
}
#endif
