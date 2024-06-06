#ifndef PEER_H
#define PEER_H

#include <string>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>

namespace Peer {

    struct PeerClient { 
        bool am_interested = false; 
        bool am_choking = true; 
        bool peer_interested = true; 
        bool peer_choking = false;
        bool shook = false;
        int socket = -1;

        std::string peer_id;
        sockaddr_in sockaddr;

        PeerClient(std::string peer_id_str, std::string ip_addr, int port);
            
        PeerClient(uint32_t ip_addr, uint16_t port);

        std::string to_string();
    };
}

#endif