#include "peer.hpp"

namespace Peer
{
    PeerClient::PeerClient(std::string pid, std::string ip_addr, int p)
    {
        inet_pton(AF_INET, ip_addr.c_str(), &(sockaddr.sin_addr));
        sockaddr.sin_port = htons(p);

        am_interested = false;
        am_choking = true;
        peer_choking = true;
        peer_interested = false;
        recv_shake = false;
        sent_shake = false;
        connected = false;
        peer_id = "";
        buffer = nullptr;
        peer_bitfield = nullptr;
        reading = false;
        outgoing_requests = 0;
    }

    PeerClient::PeerClient(uint32_t ip_addr, uint16_t p)
    {
        sockaddr.sin_family = AF_INET;
        sockaddr.sin_port = p;
        sockaddr.sin_addr.s_addr = ip_addr; // in host order!

        am_interested = false;
        am_choking = true;
        peer_choking = true;
        peer_interested = false;
        recv_shake = false;
        sent_shake = false;
        connected = false;
        peer_id = "";
        buffer = nullptr;
        peer_bitfield = nullptr;
        reading = false;
        outgoing_requests = 0;
    }

    PeerClient::PeerClient()
    {
        am_interested = false;
        am_choking = true;
        peer_choking = true;
        peer_interested = false;
        recv_shake = false;
        sent_shake = false;
        connected = false;
        peer_id = "";
        buffer = nullptr;
        peer_bitfield = nullptr;
        reading = false;
        outgoing_requests = 0;
    }

    std::string PeerClient::to_string()
    {
        char str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(sockaddr.sin_addr), str, INET_ADDRSTRLEN);

        std::string output = "peer ip addr: " + std::string(str) + " port: " + std::to_string(ntohs(sockaddr.sin_port));
        return output;
    }
}