#ifndef PEER_H
#define PEER_H

#include <string>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "message.h"

namespace Peer {

    // An unconnected, non-handshook peer client
    // these get return by the tracker manager.
    struct PeerClient { 
        int socket;
        bool am_interested; 
        bool am_choking; 
        bool peer_interested; 
        bool peer_choking;
        
        // did we handshake with this peer yet?
        bool shook; 
        // did we connect to this peer yet?
        bool connected;
        std::string peer_id;

        sockaddr_in sockaddr;
        PeerClient(std::string peer_id_str, std::string ip_addr, int port);
        PeerClient(uint32_t ip_addr, uint16_t port);
        std::string to_string();
    };

}

#endif