#ifndef PEER_H
#define PEER_H

#include <string>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "message.h"
#include "file.h"

namespace Peer
{

    struct PeerClient
    {
        int socket;           // the peer's socket
        bool am_interested;   // whether we are interested in this peer
        bool am_choking;      // whether we are choking this peer
        bool peer_interested; // whether this peer is interested in our client
        bool peer_choking;    // whether this peer is choking our client
        std::string peer_id;  // this peer's chosen id

        bool sent_shake; // did we send the handshake with this peer yet?
        bool recv_shake; // did we recv the handshake with this peer yet?
        bool connected;  // did we connect to this peer yet?
        bool reading;    // is this peer in the middle of sending a message?

        int outgoing_requests; // the number of requests that we do not have pieces for yet.
                               // We will maintain a const number of outgoing requests.

        Messages::Buffer *buffer;      // this peer's buffer that stores bytes for an incoming message
        File::BitField *peer_bitfield; // this peer's bitfield

        sockaddr_in sockaddr;                                               // this peer's socket address
        PeerClient(std::string peer_id_str, std::string ip_addr, int port); // overload for dictionary mode
        PeerClient(uint32_t ip_addr, uint16_t port);                        // overload for binary mode
        PeerClient();                                                       // for incoming connections, we don't need addr info
        std::string to_string();
    };

}

#endif