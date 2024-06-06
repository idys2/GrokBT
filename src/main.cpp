#include <iostream>

#include <sys/socket.h>
#include <netdb.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <argparse/argparse.hpp>

#include "client.h"
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

    // parse metafile and obtain announce url
    std::string metainfo = Client::read_metainfo(torrent_file);
    std::string announce_url = Client::read_announce_url(metainfo);
	sockaddr_in tracker_addr = TrackerProtocol::get_tracker_addr(announce_url);

    // get socket set up to connect to tracker
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) std::cout << "Failed to create socket to tracker" << std::endl;

    int connected = connect(sock, (sockaddr *)&tracker_addr, sizeof(sockaddr_in));
	if (connected < 0) std::cout << "Failed to connect to tracker" << std::endl;

    // create tracker request
	TrackerProtocol::TrackerRequest tracker_req(announce_url, sock); 
	
	// create tracker response 
	TrackerProtocol::TrackerResponse tracker_resp(sock);

	// hash info dict
	std::string info_dict = Client::read_info_dict_str(metainfo); 
	std::string hashed_info_dict = Client::hash_info_dict_str(info_dict);

	// set fields
	tracker_req.info_hash = hashed_info_dict;
	tracker_req.peer_id = peer_id;
	tracker_req.port = port;
	tracker_req.uploaded = "0";
	tracker_req.downloaded = "0";
	tracker_req.left = "0";
	tracker_req.compact = 1;
	tracker_req.no_peer_id = 0;
	tracker_req.event = TrackerProtocol::EventType::STARTED;

    // send HTTP request
    std::cout << "sending HTTP" << std::endl;
    tracker_req.send_http();

    // get HTTP response
    std::cout << "recv HTTP" << std::endl;
    tracker_resp.recv_http();

    for (auto& peer : tracker_resp.peers) {
        std::cout << peer.to_string() << std::endl;
    }

    return 0;
}