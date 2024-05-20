/**
 * DISCONTINUED
 * @see test_threads.cpp
*/

#include <iostream>
#include <vector>
#include <thread>
#include <string>
#include <cstring>

#include <assert.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <errno.h>

#include "tracker_protocol.h"
#include "peer_wire_protocol.h"

bool DEBUG = true;
#define DEBUGPRINTLN(x) if (DEBUG) std::cout << x << std::endl
#define on_error(x) {std::cerr << x << ": " << strerror(errno) << std::endl; exit(1); }

int main() {

    // parse metafile and obtain announce url
    std::string metainfo = ProtocolUtils::read_metainfo("debian1.torrent");
    std::string announce_url = ProtocolUtils::read_announce_url(metainfo);
    DEBUGPRINTLN("Announce URL: " + announce_url);

    // connect to tracker
	sockaddr_in tracker_addr = TrackerProtocol::get_tracker_addr(announce_url);
	int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) on_error("Failed to create socket to tracker");
	
	int connected = connect(sock, (sockaddr *)&tracker_addr, sizeof(sockaddr_in));
	if (connected < 0) on_error("Failed to connect to tracker socket");

    // create tracker request
	TrackerProtocol::TrackerRequest tracker_req(announce_url, sock); 
	
	// create tracker response 
	TrackerProtocol::TrackerResponse tracker_resp(sock);

	// hash info dict
	std::string info_dict = ProtocolUtils::read_info_dict_str(metainfo); 
	std::string hashed_info_dict = ProtocolUtils::hash_info_dict_str(info_dict);
	
	// generate unique peer id
	std::string id = "EZ6969"; 
	std::string client_id = ProtocolUtils::unique_peer_id(id);

	// set fields
	tracker_req.info_hash = hashed_info_dict;
	tracker_req.peer_id = client_id;
	tracker_req.port = 6881;
	tracker_req.uploaded = "0";
	tracker_req.downloaded = "0";
	tracker_req.left = "something";
	tracker_req.compact = 1;
	tracker_req.no_peer_id = 0;
	tracker_req.event = TrackerProtocol::EventType::STARTED;

    // send HTTP request
    DEBUGPRINTLN("Sending HTTP request...");
    tracker_req.send_http();

    // get HTTP response
    DEBUGPRINTLN("Receiving HTTP response");
    tracker_resp.recv_http();

    for (auto& peer : tracker_resp.peers) DEBUGPRINTLN(peer.str());

    /**
     * connect to peers
     */
    auto num_peers = tracker_resp.peers.size();

    // peer.str() -> struct Peer
    std::map<std::string, struct PeerWireProtocol::Peer> peers;
    
    // create a socket for each TCP peer connection
    for (auto i = 0; i < num_peers; i++) {
        auto peer_addr = tracker_resp.peers[i]; // peer as defined in `tracker_protocol.h`

        // create socket for peer
        int peer_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (peer_sock < 0) on_error("Failed to create socket for peer " + peer_addr.str());

        auto peer = PeerWireProtocol::Peer(peer_sock);  // peer as defined in `peer_wire_protocol.h`

        // insert into peers map
        peers.insert({peer_addr.str(), PeerWireProtocol::Peer(peer_sock)});

        // attempt to connect to the peer
        if (connect(peer_sock, (struct sockaddr *) &peer_addr.sockaddr, sizeof(peer_addr.sockaddr)) < 0) {
            std::cout << "Could not connect to " << peer_addr.str() + ": " << strerror(errno) << std::endl;
            peer.is_offline();
            
        } else {
            std::cout << "Connected to peer: " << peer_addr.str() << std::endl;

            // construct and send handshake request
            auto handshake = PeerWireProtocol::Handshake(tracker_req.info_hash, tracker_req.peer_id);
            handshake.send_msg(peer_sock);

            DEBUGPRINTLN("Sent handshake");

            // receive and unpack handshake response
            auto peer_handshake = PeerWireProtocol::Handshake();
            peer_handshake.recv_msg(peer_sock);

            DEBUGPRINTLN("Received handshake packet from: " + peer_addr.str() + "\n" + peer_handshake.str());
            if (handshake.info_hash != peer_handshake.info_hash) {
                peer.is_offline();
                DEBUGPRINTLN("Received nonmatching info_hash, dropping connection.");
            }
        }
    }

    /**
     * TODO: set up polling
     * Instead of receiving handshake responses as we send the requests, we will send all the requests
     * out at the same time, and then poll for the handshake responses
    */
    struct pollfd fds[num_peers];
    int counter = 0;
    for (auto& entry : peers) {
        std::string key = entry.first;
        auto peer = entry.second;

        fds[counter].fd = peer.fd;
        fds[counter].events = POLLIN;
        counter++;
    }
    
    /**
     * UNIFINISHED
     * @see test_threads.cpp
    */
    while (true) {
        for (auto i = 0; i < num_peers; i++) {
            
            int res = poll(fds, num_peers, 0);
                
            if (res == -1) {
                on_error("Could not poll");
            }

            else {
                if (fds[i].revents == POLLIN) {

                    /**
                     * Conditions for leeching:
                     * - handshake has been confirmed (fd != -1)
                     * - client is interested in the peer (am_interested = 1)
                     * - peer is not choking the client (peer_choking = 0)
                    */
                }
            }
        }
    } 
}