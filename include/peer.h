#ifndef PARSER_H
#define PARSER_H

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

        Peer(std::string peer_id_str, std::string ip_addr, int port);
            
        Peer(uint32_t ip_addr, uint16_t port);
    };
}

#endif