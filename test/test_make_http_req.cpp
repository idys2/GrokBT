// Send a get request to a bittorrent tracker

// Example: debian.torrent
// tracker: http://tracker.mywaifu.best:6969/announce
// http://tracker.mywaifu.best:6969/announce?info_hash=%8F%FE%AEV%C3%2A%B5M%99%92%E4%CB%B2%0E%F0pcz%9Cr&peer_id=-TR4050-8x0c04lgpo8t&port=51413&uploaded=0&downloaded=0&left=658505728&numwant=80&key=00A04B9A&compact=1&supportcrypto=1&event=started
#include <iostream>
#include <cstring>
#include <string>
#include <ctime>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <curl/curl.h>
#include <assert.h>
#include <unistd.h>

#include "tracker_protocol.h"
#include "protocol_utils.h"
#include <sstream>
using namespace std;

int main() {

	// parse metainfo file
	string metainfo = ProtocolUtils::read_metainfo("debian1.torrent");
	string announce_url = ProtocolUtils::read_announce_url(metainfo);
	cout << "Announce URL: \n" << announce_url << endl;

	// get the address struct for the tracker
	sockaddr_in tracker_addr = TrackerProtocol::get_tracker_addr(announce_url);

	int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); 
	assert(sock >= 0); 

	// connect to tracker
	int connected = connect(sock, (sockaddr *)&tracker_addr, sizeof(sockaddr_in));
	assert(connected >= 0);

	// create tracker request
	TrackerProtocol::TrackerRequest tracker_req(announce_url, sock); 
	
	// create tracker response 
	TrackerProtocol::TrackerResponse tracker_resp(sock);

	// hash info dict
	string info_dict = ProtocolUtils::read_info_dict_str(metainfo); 
	string hashed_info_dict = ProtocolUtils::hash_info_dict_str(info_dict);
	
	// generate unique peer id
	string client_id = "EZ6969"; 
	string peer_id = ProtocolUtils::unique_peer_id(client_id);

	// set fields
	tracker_req.info_hash = hashed_info_dict;
	tracker_req.peer_id = peer_id;
	tracker_req.port = 6881;
	tracker_req.uploaded = "0";
	tracker_req.downloaded = "0";
	tracker_req.left = "something";
	tracker_req.compact = 1;
	tracker_req.no_peer_id = 0;
	tracker_req.event = TrackerProtocol::EventType::STARTED;
	
	// string req = tracker_req.construct_http_string();
	// cout << "Request: \n" << req << endl;
	
	//------------- Send
	cout << "Sending..." << endl;
	tracker_req.send_http();
	
	cout << "Receiving..." << endl;
	tracker_resp.recv_http();

	// Print out all peers we got
	cout << "Printing all peers..." << endl;
	for (auto& peer : tracker_resp.peers) {
		cout << peer.str() << endl;
	}

	return 0; 
}
