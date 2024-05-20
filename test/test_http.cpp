/**
 * TODO: Goal for communicating with the tracker:
 * - craft HTTP GET requests using libcurl
 * - create socket to tracker
 * - send HTTP GET requests via socket (not using libcurl)
 * - receive and print HTTP GET response
*/

// Send a get request to a bittorrent tracker

// Example: debian.torrent
// tracker: http://tracker.mywaifu.best:6969/announce
// http://tracker.mywaifu.best:6969/announce?info_hash=%8F%FE%AEV%C3%2A%B5M%99%92%E4%CB%B2%0E%F0pcz%9Cr&peer_id=-TR4050-8x0c04lgpo8t&port=51413&uploaded=0&downloaded=0&left=658505728&numwant=80&key=00A04B9A&compact=1&supportcrypto=1&event=started
#include <iostream>
#include <string>

#include <sys/socket.h>
#include <netdb.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <curl/curl.h>
#include <assert.h>

#include "tracker_protocol.h"

int main() {

	// get the ip address of the tracker
	std::string announce_url = "http://tracker.mywaifu.best:6969/announce";
	sockaddr_in tracker_addr = TrackerProtocol::get_tracker_addr(announce_url);

	int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); 
	assert(sock >= 0); 
	
	int connected = connect(sock, (sockaddr *)&tracker_addr, sizeof(sockaddr_in));
	assert(connected >= 0);


	// test Tracker Req (most fields uninitialized)
	TrackerProtocol::TrackerRequest req(announce_url, sock);

	std::cout << req.announce_url << std::endl;
	std::string httpreq = req.construct_http_string();
	std::cout << httpreq << std::endl;

	return 0; 
}
