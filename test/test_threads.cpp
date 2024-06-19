#include <iostream>
#include <cstring>
#include <string>
#include <thread>

#include <assert.h>
#include <sys/socket.h>
#include <netdb.h>
/**
 * LEGACY CODE
 * @see main.cpp
 */
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <argp.h>
#include <fcntl.h>

#include "client.h"
#include "tracker_protocol.h"
#include "peer_wire_protocol.h"

#define TIMEOUT 120

std::string torrent_file_path;
const std::string id = "EZ6969";
const int listen_port = 6881;
const int MAX_LISTEN_Q = 50;

#define MAX_EVENTS 128
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
            if (pair.second.fd != -1 && (current_time - pair.second.timestamp > (time_t)TIMEOUT)) { // if peer hasn't responded in TIMEOUT seconds
                torrent.peers.at(pair.first).timestamp = time(nullptr);
                keep_alive.send_msg(pair.first);
            }
        }
    }
}

/**
 * TODO: Handle all communication between listener and peers
 * When a leecher connects to us, we start seeding
 * 1. accept() connection
 * 2. receive handshake to get info
 * 3. 
*/
void listener_poll() {
    DEBUGPRINTLN("Started listener poll");

    struct epoll_event ev;
    int fd, nfds, leech_sock;
    listen_epfd = epoll_create1(0);
    if (listen_epfd == -1) on_error("Could not create epoll fd");

    struct sockaddr_in leech;
    socklen_t leechlen = sizeof(leech);

    // poll indefinitely for incoming connections
    for (;;) {

        nfds = epoll_wait(listen_epfd, events, MAX_EVENTS, -1); 
        if (nfds == -1) on_error("Could not wait for events in listener poll");

        for (auto i = 0; i < nfds; ++i) {
            fd = events[i].data.fd;
            
            // on connection request
            if (fd == listen_sock) {

                // accept new connection
                leech_sock = accept(listen_sock, (struct sockaddr *) &leech, &leechlen);
                if (leech_sock == -1) on_error("Could not accept connection in listen poll");

                // add connection
                ev.events = EPOLLIN;
                ev.data.fd = leech_sock;
                if (epoll_ctl(listen_epfd, EPOLL_CTL_ADD, leech_sock, &ev) == -1) on_error("Could not add new leech to listen epoll");
            } 
            
            else {

                // respond to handshake requests
                if (events[i].events & EPOLLIN) {
                    
                    // TODO: what if it isn't a handshake?

                    // receive handshake
                    auto handshake_req = PeerWireProtocol::Handshake(torrent.info_hash, torrent.client_id);
                    handshake_req.recv_msg(fd);

                    // send back handshake 
                    auto handshake_resp = PeerWireProtocol::Handshake(torrent.info_hash, torrent.client_id);
                    handshake_resp.send_msg(fd);
                    
                    // send our bitfield
                    auto bitfield_msg = PeerWireProtocol::Bitfield(torrent.info_dict.piece_bitfield.bitfield.size());
                    bitfield_msg.send_msg(fd);

                    // unchoke the peer
                    auto unchoke_msg = PeerWireProtocol::Unchoke();
                    unchoke_msg.send_msg(fd);

                    // add fd to peer poll, and remove from listener poll
                    ev.data.fd = fd;
                    ev.events = EPOLLIN;
                    if (epoll_ctl(peer_epfd, EPOLL_CTL_ADD, leech_sock, &ev) == -1) on_error("Could not add new leech to peer epoll");
                    if (epoll_ctl(listen_epfd, EPOLL_CTL_DEL, leech_sock, &ev) == -1) on_error("Could not remove leech from listener epoll");
                    
                    // add to map
                    //torrent.map_mutex.lock();
                    torrent.peers.insert({fd, ProtocolUtils::Peer(leech.sin_addr.s_addr, leech.sin_port)});
                    //torrent.map_mutex.unlock();
                }
            }
        }
    }
}

/**
 * TODO: Handle all communication between client and peers
*/
void peer_poll(std::vector<ProtocolUtils::Peer> init_peers) {
    DEBUGPRINTLN("Started peer poll");

    struct epoll_event ev;
    int fd, nfds;
    peer_epfd = epoll_create1(0);
    if (peer_epfd == -1) on_error("Could not creat epoll for peers");

    for (auto& peer_entry : init_peers) {

        // create nonblocking socket for peer
        int peer_sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);
        if (peer_sock < 0) on_error("Failed to create socket for peer");

        int connect_peer_sock = connect(peer_sock, (struct sockaddr *) &peer_entry.sockaddr, sizeof(peer_entry.sockaddr));

        ev.events = EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLPRI | EPOLLERR | EPOLLHUP;
        ev.data.fd = peer_sock;

        if (epoll_ctl(peer_epfd, EPOLL_CTL_ADD, peer_sock, &ev) == -1) {
            on_error("Could not add " + peer_entry.str() + "to epoll");
        }
        
        //torrent.map_mutex.lock();
        torrent.peers.insert({peer_sock, peer_entry});
        //torrent.map_mutex.unlock();
    }

    // start indefinite polling of peers
    while (true) {

        nfds = epoll_wait(peer_epfd, events, MAX_EVENTS, -1);
        if (nfds == -1) on_error("Could not wait for events in peer poll");

        for (auto i = 0; i < nfds; i++) {
            fd = events[i].data.fd;
            //ProtocolUtils::Peer peer = torrent.peers.at(fd);
            
            if (events[i].events & (EPOLLRDHUP | EPOLLPRI | EPOLLERR | EPOLLHUP)) {
                torrent.peers.at(fd).is_inactive();
                if (epoll_ctl(peer_epfd, EPOLL_CTL_DEL, fd, &ev) == -1) on_error("Error removing");
            }

            else if (events[i].events & (EPOLLOUT | EPOLLIN)) {

                if (torrent.peers.at(fd).shook) {
                    // receive first 4 bytes to determine length
                    uint32_t len = 0;
                    assert(recv(fd, &len, sizeof(len), MSG_WAITALL) > 0);
                    len = ntohl(len);
                    DEBUGPRINTLN("Length of received message: " + std::to_string(len));

                    if (len > 0) {
                        
                        // receive rest of message (ID + payload)
                        uint8_t *buf = new uint8_t[len];
                        //recv(fd, buf, len, 0);
                        //int recv_bytes = 0;
                        //while (recv_bytes < len) recv_bytes += recv(fd, buf + recv_bytes, len - recv_bytes, 0);
                        assert(recv(fd, buf, len, MSG_WAITALL) == len);    // wait for the full message to arrive
                        uint8_t buf_id = buf[0];

                        DEBUGPRINTLN("Received ID: " + std::to_string(buf_id));

                        // cases for IDs
                        if (buf_id == PeerWireProtocol::CHOKE_ID) {
                            DEBUGPRINTLN("\nGot choke");
                            torrent.handle_choke(fd);
                        } 
                        
                        else if (buf_id == PeerWireProtocol::UNCHOKE_ID) {  
                            DEBUGPRINTLN("\nGot unchoke");

                            torrent.handle_unchoke(fd);
                        } 
                        
                        else if (buf_id == PeerWireProtocol::INTERESTED_ID) { 
                            DEBUGPRINTLN("\nGot interested");
                            torrent.handle_interested(fd);
                        } 
                        
                        else if (buf_id == PeerWireProtocol::UNINTERESTED_ID) { 
                            DEBUGPRINTLN("\nGot uninterested");
                            torrent.handle_uninterested(fd);
                        } 
                        
                        else if (buf_id == PeerWireProtocol::HAVE_ID) { 
                            DEBUGPRINTLN("\nGot have");
                        } 
                        
                        else if (buf_id == PeerWireProtocol::BITFIELD_ID) { 
                            DEBUGPRINTLN("\nGot bitfield");
                            torrent.handle_bitfield(torrent.peers.at(fd), &buf[1], len - sizeof(buf_id));
                        } 
                        
                        else if (buf_id == PeerWireProtocol::REQUEST_ID) { 
                            DEBUGPRINTLN("\nGot request");
                            PeerWireProtocol::Request req_msg;
                            req_msg.unpack(buf, len);
                            torrent.handle_request(fd);
                        } 
                        
                        else if (buf_id == PeerWireProtocol::PIECE_ID) { 
                            DEBUGPRINTLN("\nGot piece");
                            PeerWireProtocol::Piece piece_msg;
                            piece_msg.unpack(buf, len);
                            torrent.handle_piece(fd, piece_msg);
                        } 
                        
                        // no
                        else if (buf_id == PeerWireProtocol::CANCEL_ID) {
                            DEBUGPRINTLN("\nGot cancel");
                            torrent.handle_cancel();
                        } 
                        
                        else if (buf_id == PeerWireProtocol::PORT_ID) {
                            DEBUGPRINTLN("\nGot port");
                            torrent.handle_port();
                        } else DEBUGPRINTLN("Received unknown ID: " + std::to_string(buf_id));

                        delete[] buf;
                    } else DEBUGPRINTLN("Recieved keepalive from " + torrent.peers.at(fd).str());
                } 
                
                else {
                    if (events[i].events & EPOLLOUT) {
                        struct epoll_event mv;
                        mv.events = EPOLLIN;
                        mv.data.fd = fd;
                        if (epoll_ctl(peer_epfd, EPOLL_CTL_MOD, fd, &mv) == -1) on_error("Could not modify socket");

                        PeerWireProtocol::Handshake shake = PeerWireProtocol::Handshake(torrent.info_hash, torrent.client_id);
                        shake.send_msg(fd);
                    } 
                    else if (events[i].events & EPOLLIN) {
                        PeerWireProtocol::Handshake shake = PeerWireProtocol::Handshake(torrent.info_hash, torrent.client_id);
                        shake.recv_msg(fd);
                        // check if the info hash is the same
                        // if not, remove from our map
                        
                        if(torrent.info_hash != shake.info_hash) {
                            DEBUGPRINTLN("Peer does not serve file we want");
                            torrent.peers.erase(fd);
                            if (epoll_ctl(peer_epfd, EPOLL_CTL_DEL, fd, &ev) == -1) on_error("Error removing");
                        }
                        else {
                            torrent.peers.at(fd).shook = true;

                            // set socket to nonblocking
                            int flags = fcntl(fd, F_GETFL, 0);
                            flags &= ~O_NONBLOCK;
                            fcntl(fd, F_SETFL, flags);
                        }
                    }
                }
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

int main(int argc, char* argv[]) {

    // Loop through command-line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-f" && i + 1 < argc) {
            torrent_file_path += argv[++i];
        } else {
            std::cerr << "Unknown option: " << arg << std::endl;
            return 1;
        }
    }

    // Check if filename is provided
    if (torrent_file_path.empty()) {
        std::cerr << "Usage: " << argv[0] << " -f <filename>" << std::endl;
        return 1;
    }

    // sock for listening to new client connections
    listen_sock = get_listener(listen_port);

    // generate unique peer id
	std::string client_id = ProtocolUtils::unique_peer_id(id);

    // create torrent object
    torrent = BitTorrentClient::Torrent(torrent_file_path, client_id, listen_port);  // hardcoded output!
    DEBUGPRINTLN("Created torrent object with client ID: " + torrent.client_id);

    // request list of peers from tracker (also initiates tracker communication)
    torrent.report_to_tracker(TrackerProtocol::EventType::STARTED);
    DEBUGPRINTLN("Sent first HTTP request to tracker");
    
    // receive inital list of peers from tracker
    TrackerProtocol::TrackerResponse tracker_resp(torrent.tracker_sock);
    tracker_resp.recv_http();

    // start threads to handle p2p and timeout refresh
    DEBUGPRINTLN("Starting threads..");
    std::thread peer_thread(peer_poll, torrent.update_from_tracker(tracker_resp));
    //std::thread listener_thread(listener_poll);
    //std::thread timeout_thread(timeout);
    
    peer_thread.detach();
    //listener_thread.detach();
    //timeout_thread.detach();

    while (torrent.left > 0) {
        
    }
    DEBUGPRINTLN("Done downloading file!");

    /**
     * TODO: Handle all communication between client and tracker
    */
    
    while (true) {
        //PREFORM AT THE REGULAR INTERVAL SPECIFIED BY THE TRACKER SERVER
        sleep(torrent.interval);
        //Report to tracker using EMPTY EVENT TYPE SINCE ITS THE SAME AS UNSPECIFIED EVENT TYPE
        torrent.report_to_tracker(TrackerProtocol::EventType::EMPTY);

        //RETRIEVE THE RESPONSE FROM TRACKER server
        TrackerProtocol::TrackerResponse tracker_resp2(torrent.tracker_sock);
        tracker_resp2.recv_http();

        

    }
}