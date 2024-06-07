#include <iostream>

#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

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

    for (auto& peer : tracker.peers) {
        std::cout << peer.to_string() << std::endl;
    }

    return 0;
}