#ifndef TRACKER_PROTOCOL_H
#define TRACKER_PROTOCOL_H

#include <string>
#include <curl/curl.h>

#include "bencode_wrapper.hpp"
#include "peer.h"
#include "metainfo.h"
#include "net_utils.h"

namespace TrackerProtocol {
    const int HTTP_HEADER_MAX_SIZE = 10 * 1024; // set an arbitrary maximum size for an HTTP header from tracker
	const int HTTP_MAX_SIZE = 10 * 1024; // set an arbitrary maximum size for an HTTP message from tracker
	const int HTTP_OK = 200; // status code for HTTP OK
	
	sockaddr_in get_tracker_addr(const std::string announce_url);

    // Event types for a tracker request that is sent by the client -> tracker
	enum EventType {
		STARTED, 
		STOPPED,
		COMPLETED,
		EMPTY // same as unspecified
	};

	struct TrackerManager { 

		// Request fields
		std::string info_hash;
		std::string peer_id;
		std::string uploaded;
		std::string downloaded;
		std::string left;
		std::string announce_url; 

		int client_port; 			// port number that the client is listening on, must be between 6881-6889
		int compact;		
		int no_peer_id; 	

		EventType event; 	

		// Response fields
		std::string failure_reason; 
		std::string tracker_id;  

		int interval;  
		int min_interval;
		int num_seeders;  
		int num_leechers;  
		
		std::vector<Peer::PeerClient> peers; // peers we got from a response
		
		// tcp stuff 
		int sock;
		sockaddr_in tracker_addr;

		// bool flags for state
		bool sent_completed;

		TrackerManager(std::string torrent_file, std::string p_id, int c_port);

		// given the fields for this Tracker, construct an HTTP string 
		// that will be sent
		std::string construct_http_string();

		// given an HTTP response packed into a string, parse and 
		// update fields for this Tracker
		void parse_payload(std::string payload);

		// send the HTTP request over the socket
		void send_http();

		// recv the HTTP response over the socket
		void recv_http();
	};
}


#endif