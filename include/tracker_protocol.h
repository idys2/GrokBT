#ifndef TRACKER_PROTOCOL_H
#define TRACKER_PROTOCOL_H

#include <string>
#include <curl/curl.h>

#include "bencode.hpp"
#include "peer.h"
#include "tcp_utils.h"

namespace TrackerProtocol {
    const int HTTP_HEADER_MAX_SIZE = 10 * 1024; // set an arbitrary maximum size for an HTTP header from tracker
	const int HTTP_MAX_SIZE = 10 * 1024; // set an arbitrary maximum size for an HTTP message from tracker
	const int HTTP_OK = 200; // status code for HTTP OK

    sockaddr_in get_tracker_addr(const std::string announce_url);

    // Split a string on a delimiter sequence into a list of std::strings
	// this function helps parse HTTP responses into fields based on CRLF="\r\n"
	std::vector<std::string> split(std::string input, std::string delimiter);

    // Event types for a tracker request that is sent by the client -> tracker
	enum EventType {
		STARTED, 
		STOPPED,
		COMPLETED,
		EMPTY // same as unspecified
	};

    struct TrackerRequest {

		std::string info_hash; 	// urlencoded 20 byte SHA1 hash of bencoded dictionary
		std::string peer_id; 	// urlencoded 20 byte std::string used as unique ID for client
		std::string uploaded; 	// total amount uploaded since client sent start event to tracker in base 10 ascii
		std::string downloaded;  // total amount downloaded since client sent start event to tracker in base 10 ascii
		std::string left;		// number of bytes remaining to download in base 10 ascii

		int port; 			// port number that the client is listening on, must be between 6881-6889
		int compact;		// whether or not the client accepts a "compact" response, 0 or 1
		int no_peer_id; 	// indicates tracker can omit peer id field in peers dict (ignored if compact=true), 0 or 1
		EventType event; 	// the type of event we are sending to the tracker

		std::string announce_url; // the announce url for this tracker request
		int socket; 			  // the socket to send on
		
		TrackerRequest(std::string ann_url, int sock);

		std::string construct_http_string();
		
		void send_http();
	};

    struct TrackerResponse {
		
		int socket;
		std::string http_resp;  // the raw HTTP response
		std::string bencoded_payload; // the raw payload of the tracker response that we received on the socket
		std::string failure_reason = ""; // tracker gave an error, not always available
		int interval;  // interval in seconds that the client should wait between regular requests 
		std::string tracker_id;  // std::string id that client should send back on next announcements, if provided
		int num_seeders;  // number of clients with entire file 
		int num_leechers;  // number of clients who arent seeders
		
		std::vector<Peer::PeerClient> peers; // peers we got from this response

		TrackerResponse(int sock);

		void recv_http();
		
		void parse_payload(std::string payload);
	};
}


#endif