#include <iostream>

#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <poll.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>

#include <argparse/argparse.hpp>

#include "client.h"
#include "metainfo.h"
#include "tracker_protocol.h"

int main(int argc, char* argv[]) {
    // get arguments from command line

    argparse::ArgumentParser program("client");
    std::string torrent_file;
    std::string client_id;
    int port;

    program.add_argument("-f").default_value("debian1.torrent").store_into(torrent_file);
    program.add_argument("-id").default_value("EZ6969").store_into(client_id);
    program.add_argument("-p").default_value(6881).store_into(port);
    try {
        program.parse_args(argc, argv);
    }
    catch (const std::exception& err) {
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        std::exit(1);
    };

    // generate unique peer id
	std::string peer_id = Client::unique_peer_id(client_id);

    TrackerProtocol::TrackerManager tracker(torrent_file, peer_id, port);
    tracker.send_http();
    tracker.recv_http();

    // get all base peers from the tracker
    std::vector<Peer::PeerClient> peers = tracker.peers;
    int fd_count = peers.size();
    pollfd *pfds = new pollfd[fd_count];

    // create socket for all peers, and set to nonblocking
    // connect on these sockets
    for(int i = 0; i < fd_count; i++) {
        int peer_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        fcntl(peer_sock, F_SETFL, O_NONBLOCK);
        pfds[i].fd = peer_sock;
        pfds[i].events = POLLIN | POLLOUT;
        connect(peer_sock, (sockaddr *)(&peers[i].sockaddr), sizeof(sockaddr_in));
    }

    // main poll event loop
    while(true) {
        int poll_peers = poll(pfds, fd_count, 5 * 1000);
        
        for(int i = 0; i < fd_count; i++) {
            
            // any client that responded here are now connected
            // now we need to handshake
            if(pfds[i].revents & (POLLIN | POLLOUT)) {
                std::cout << peers[i].to_string() << std::endl;
            }


        }    
    }

    delete[] pfds;


    return 0;
}