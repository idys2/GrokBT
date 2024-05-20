/**
 * OUTDATED
 * @see test_thread.cpp
*/

#include <iostream>
#include <cstring>
#include <string>
#include <thread>

#include <assert.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/epoll.h>

#include "client.h"
#include "tracker_protocol.h"
#include "peer_wire_protocol.h"

#define TIMEOUT 120

const std::string torrent_file_path = "debian1.torrent";
const std::string id = "EZ6969";
const int listen_port = 6881;
const int MAX_LISTEN_Q = 50;

#define MAX_EVENTS 64
struct epoll_event events[MAX_EVENTS];

int listen_sock, peer_epfd, listen_epfd;
BitTorrentClient::Torrent torrent;
size_t num_peers;

/**
 * TODO: Periodically send keepalive messages
*/
void timeout() {
    auto keep_alive = PeerWireProtocol::KeepAlive();
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(TIMEOUT));

        for (auto const& pair : torrent.peers) {
            time_t current_time = time(nullptr);
            if (pair.first != -1 && (current_time - pair.second.timestamp > (time_t)TIMEOUT)) { // if peer hasn't responded in TIMEOUT seconds
                torrent.peers[pair.first].timestamp = time(nullptr);
                keep_alive.send_msg(pair.first);
            }
        }
        
    }
}

/**
 * TODO: Handle all communication between listener and peers
*/
void listener_poll() {

    int fd, nfds;
    listen_epfd = epoll_create1(0);
    if (listen_epfd == -1) on_error("Could not create epoll fd");

    // start initial peer connections
    // TODO: lock the map
    for (auto& peer_entry : torrent.peers) {
        int peer_fd = peer_entry.first;
        ProtocolUtils::Peer peer = peer_entry.second;

        struct epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.fd = listen_sock;

        if (epoll_ctl(listen_epfd, EPOLL_CTL_ADD, listen_sock, &ev) == -1) 
            on_error("Could not add " + peer.str() + "to epoll");
    }

    // TODO: accept() connections
}

/**
 * TODO: Handle all communication between client and peers
*/
void peer_poll() {
    int fd, nfds;
    peer_epfd = epoll_create1(0);
    if (peer_epfd == -1) on_error("Could not creat epoll for peers");
    
    // start initial peer connections
    // TODO: lock the map
    for (auto& peer_entry : torrent.peers) {
        int peer_fd = peer_entry.first;
        ProtocolUtils::Peer peer = peer_entry.second;

        struct epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.fd = peer_fd;

        if (epoll_ctl(peer_epfd, EPOLL_CTL_ADD, listen_sock, &ev) == -1) on_error("Could not add " + peer.str() + "to epoll");
    }

    // start indefinite polling of peers
    while (torrent.left > 0) {

        nfds = epoll_wait(peer_epfd, events, MAX_EVENTS, -1);
        if (nfds == -1) on_error("Could not wait for events");

        for (auto i = 0; i < nfds; ++i) {
            if (events[i].events & EPOLLIN) {
                fd = events[i].data.fd;
                ProtocolUtils::Peer peer = torrent.peers.at(fd);

                // receive first 4 bytes to determine length
                uint64_t len;
                recv(fd, &len, sizeof(len), 0);

                if (len != 0) {

                    // receive rest of the message
                    char *buf = new char[len];
                    int retmungus = recv(fd, buf, len, MSG_WAITALL);
                    //This is here to see if the message is actually fully being sent or not
                    assert(retmungus == len);

                    uint8_t buf_id = buf[4];
                    
                    switch (buf_id) {
                        case PeerWireProtocol::CHOKE_ID:
                            DEBUGPRINTLN("Got choke");
                            torrent.handle_choke(fd);
                            break;
                            
                        case PeerWireProtocol::UNCHOKE_ID:
                            DEBUGPRINTLN("Got unchoke");
                            torrent.handle_unchoke(fd);
                            break;

                        case PeerWireProtocol::INTERESTED_ID:
                            DEBUGPRINTLN("Got interested");
                            torrent.handle_interested(fd);
                            break;

                        case PeerWireProtocol::UNINTERESTED_ID:
                            DEBUGPRINTLN("Got uninterested");
                            torrent.handle_uninterested(fd);
                            break;

                        case PeerWireProtocol::HAVE_ID:
                            DEBUGPRINTLN("Got have");
                            torrent.handle_have();
                            break;

                        case PeerWireProtocol::BITFIELD_ID:
                            DEBUGPRINTLN("Got bitfield");
                            torrent.handle_bitfield();
                            break;

                        case PeerWireProtocol::REQUEST_ID:
                            DEBUGPRINTLN("Got request");
                            torrent.handle_bitfield();
                            break;

                        case PeerWireProtocol::PIECE_ID:
                            DEBUGPRINTLN("Got piece");
                            torrent.handle_piece();
                            break;

                        case PeerWireProtocol::CANCEL_ID:
                            DEBUGPRINTLN("Got cancel");
                            torrent.handle_cancel();
                            break;

                        case PeerWireProtocol::PORT_ID:
                            DEBUGPRINTLN("Got port");
                            torrent.handle_port();
                            break;
                        
                        default:
                            DEBUGPRINTLN("Received unknown ID: " + buf_id);
                            break;
                    }
                } else DEBUGPRINTLN("Recieved keepalive from " + torrent.peers.at(fd).str());
            }
        }
    }
}

// from Beej's guide
int get_listener(int port){
    int listener;
    int on = 1;
    int rv;

    addrinfo hint, *ai, *p;

    memset(&hint,0,sizeof hint);
    hint.ai_family = AF_UNSPEC;
    hint.ai_socktype = SOCK_STREAM;
    hint.ai_flags = AI_PASSIVE;


    char const* port_str = std::to_string(port).c_str();
    if ((rv = getaddrinfo(NULL, port_str, &hint, &ai)) != 0) {
		fprintf(stderr, "selectserver: %s\n", gai_strerror(rv));
		exit(1);
	}

	for(p = ai; p != NULL; p = p->ai_next) {
		listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (listener < 0) { 
			continue;
		}
		
		// Lose the pesky "address already in use" error message
		setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(int));

		if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
			close(listener);
			continue;
		}

		break;
	}

	freeaddrinfo(ai); 

	if(p == NULL) {
		return -1;
	}

	if(listen(listener, MAX_LISTEN_Q) == -1) { // guess and allow 50 clients to queue for listening
		return -1; 
    }
	return listener; 
}

int main() {
    
    // sock for listening to new client connections
    listen_sock = get_listener(listen_port);

    // generate unique peer id
	std::string client_id = ProtocolUtils::unique_peer_id(id);

    // create torrent object
    torrent = BitTorrentClient::Torrent(torrent_file_path, "out.txt", client_id, listen_port);  // hardcoded output!

    // request list of peers from tracker (also initiates tracker communication)
    torrent.report_to_tracker(TrackerProtocol::EventType::STARTED);
    
    // receive inital list of peers from tracker
    TrackerProtocol::TrackerResponse tracker_resp(torrent.tracker_sock);
    tracker_resp.recv_http();
    torrent.update_from_tracker(tracker_resp);

    // start threads to handle p2p and timeout refresh
    std::thread peer_thread(peer_poll);
    std::thread listener_thread(listener_poll);
    //std::thread timeout_thread(timeout);
    
    peer_thread.detach();
    listener_thread.detach();
    //timeout_thread.detach();

    /**
     * TODO: Handle all communication between client and tracker
    */

}