#include "peer.h"

Peer::PeerClient::PeerClient(std::string pid, std::string ip_addr, int p) {
    inet_pton(AF_INET, ip_addr.c_str(), &(sockaddr.sin_addr));
    sockaddr.sin_port = htons(p); 
}

Peer::PeerClient::PeerClient(uint32_t ip_addr, uint16_t p) {
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = p; 
    sockaddr.sin_addr.s_addr = ip_addr; // in host order!
}

std::string Peer::PeerClient::to_string() {
    char str[INET_ADDRSTRLEN]; 
    inet_ntop(AF_INET, &(sockaddr.sin_addr), str, INET_ADDRSTRLEN); 

    std::string output = "peer ip addr: "+ std::string(str) + " port: " + std::to_string(ntohs(sockaddr.sin_port)); 
    return output;
}