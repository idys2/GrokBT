/*
* This file defines functions used in communicating with the tracker server using http
*/

#ifndef _TRACKER_PROTOCOL_H
#define _TRACKER_PROTOCOL_H

#include <cstring>
#include <string>
#include <iostream>
#include <sstream>
#include <vector>

#include <curl/curl.h>
#include "bencode.hpp"
#include "protocol_utils.h"

namespace TrackerProtocol {
	
	const int HTTP_HEADER_MAX_SIZE = 10 * 1024; // set an arbitrary maximum size for an HTTP header from tracker
	const int HTTP_MAX_SIZE = 10 * 1024; // set an arbitrary maximum size for an HTTP message from tracker
	const int HTTP_OK = 200; // status code for HTTP OK

	// get the tracker address struct given the announce url of the tracker
	// the announce url looks something like this: 
	// http://tracker.mywaifu.best:6969/announce
	sockaddr_in get_tracker_addr(const std::string announce_url) {
		// get the host part and port of the announce url
		char *host;
		char *port;
		CURLU *handle = curl_url();
		CURLUcode rc = curl_url_set(handle, CURLUPART_URL, announce_url.c_str(), 0); 		
		rc = curl_url_get(handle, CURLUPART_HOST, &host, 0); 
		rc = curl_url_get(handle, CURLUPART_PORT, &port, 0); 

		std::string host_url(host);
		std::string port_str(port);
		curl_url_cleanup(handle);

		// call getaddrinfo, return the first valid ip address we find
		int status; 
		addrinfo hints; 
		addrinfo *servinfo; 
		memset(&hints, 0, sizeof hints);

		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM; 
		hints.ai_flags = AI_PASSIVE; 
		
		if ((status = getaddrinfo(host_url.c_str(), port_str.c_str(), &hints, &servinfo)) != 0) {
			fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
			exit(1);
		}
		
		sockaddr_in *tracker_addr = (sockaddr_in *)(servinfo -> ai_addr);
		freeaddrinfo(servinfo);
		return *tracker_addr;
	}
	
	// Split a string on a delimiter sequence into a list of std::strings
	// this function helps parse HTTP responses into fields based on CRLF="\r\n"
	std::vector<std::string> split(std::string input, std::string delimiter) {
		std::vector<std::string> tokens; 

		std::string token;
		size_t last_break_end = 0; 
		size_t found_pos = input.find(delimiter, last_break_end);
		// find delimiter in the string, substring input between 
		// last occurrenace of the delimiter and new occurrence
		// add as token
		while(found_pos != std::string::npos) {
			size_t substr_len = found_pos - last_break_end + 1; 
			std::string token = input.substr(last_break_end, substr_len);
			tokens.push_back(token);
			last_break_end = found_pos + delimiter.length(); //need to advance past the delimiter
			found_pos = input.find(delimiter, last_break_end); 
		}

		// add last token, which should run from last_break_end to end of std::string
		tokens.push_back(input.substr(last_break_end, std::string::npos));

		return tokens;
	}

	// Event types for a tracker request that is sent by the client -> tracker
	enum EventType {
		STARTED, 
		STOPPED,
		COMPLETED,
		EMPTY // same as unspecified
	};

	// TrackerRequest is a struct that holds all relevant information about a request being made to 
	// the Tracker. It implicitly manipulates a libcurl URL for the client
	// with the provided information, crafts a HTTP GET request, and sends to the tracker
	struct TrackerRequest {

		std::string info_hash; 	// urlencoded 20 byte SHA1 hash of bencoded dictionary
		std::string peer_id; 	// urlencoded 20 byte std::string used as unique ID for client
		int port; 			// port number that the client is listening on, must be between 6881-6889
		std::string uploaded; 	// total amount uploaded since client sent start event to tracker in base 10 ascii
		std::string downloaded;  // total amount downloaded since client sent start event to tracker in base 10 ascii
		std::string left;		// number of bytes remaining to download in base 10 ascii
		int compact;		// whether or not the client accepts a "compact" response, 0 or 1
		int no_peer_id; 	// indicates tracker can omit peer id field in peers dict (ignored if compact=true), 0 or 1
		EventType event; 	

		// fields for sending to tracker
		std::string announce_url; // the announce url from the metainfo file
		int socket; 		 // the socket we are using to communicate with the tracker, bound at port 
		

		TrackerRequest(std::string ann_url, int sock) {
			announce_url = ann_url;
			socket = sock;
		};

		// Function to craft HTTP GET request using the provided fields
		// manually constructs the HTTP request
		// assumes all fields are defined.
		std::string construct_http_string() {
			// get the host part of the announce URL
			CURLU *handle = curl_url();
			CURLUcode rc = curl_url_set(handle, CURLUPART_URL, announce_url.c_str(), 0); 
			char *host; 
			rc = curl_url_get(handle, CURLUPART_HOST, &host, 0); 

			std::string host_url(host);
			curl_url_cleanup(handle);

			// url encoding
			CURL *curl = curl_easy_init();
			assert(curl); // should not fail!

			char *url_encoded_info = curl_easy_escape(curl, info_hash.c_str(), info_hash.length());
			char *url_encoded_peer_id = curl_easy_escape(curl, peer_id.c_str(), peer_id.length());

			// "GET /announce?param=value&param=value HTTP/1.1\r\nHost: {}\r\n\r\n"
			std::string request = "";
			request += "GET /announce?";
			request += "info_hash=" + std::string(url_encoded_info) + "&"; // urlencoded
			request += "peer_id=" + std::string(url_encoded_peer_id) + "&"; // urlencoded
			request += "port=" + std::to_string(port) + "&";
			request += "uploaded=" + uploaded + "&";
			request += "downloaded=" + downloaded + "&";
			request += "left=" + left + "&";
			request += "compact=" + std::to_string(compact) + "&";
			request += "no_peer_id=" + std::to_string(no_peer_id) + "&";
			
			switch(event) {

				// must be sent first
				case STARTED:
					request += "event=started&";
					break; 

				// client is shutting down gracefully
				case STOPPED: 
					request += "event=stopped&";
					break; 

				// client has completed the download
				case COMPLETED:
					request += "event=completed&";
					break;

				// performed at regular intervals, omit here
				case EMPTY: 
					break;
					
			}
			
			// add HTTP version
			request += " HTTP/1.1\r\n";

			// add host std::string
			request += "Host: " + host_url + "\r\n\r\n";

			return request;
		}
		
		// Pack all headers into a std::string, transmit the HTTP request with TCP
		void send_http() {
			std::string http_req = construct_http_string();
			uint32_t length = http_req.length();
			assert(ProtocolUtils::sendall(socket, http_req.c_str(), &length) == 0); 
		}
	};
	
	// TrackerResponse receives an HTTP response from the tracker, 
	// and abstracts away parsing both the HTTP header fields, and the bencoded payload 
	// to extract a vector of peer clients
	struct TrackerResponse {
		
		int socket;
		std::string http_resp;  // the raw HTTP response
		std::string bencoded_payload; // the raw payload of the tracker response that we received on the socket
		std::string failure_reason = ""; // tracker gave an error, not always available
		int interval;  // interval in seconds that the client should wait between regular requests 
		std::string tracker_id;  // std::string id that client should send back on next announcements, if provided
		int num_seeders;  // number of clients with entire file 
		int num_leechers;  // number of clients who arent seeders
		
		std::vector<ProtocolUtils::Peer> peers; // peers we got from this response

		TrackerResponse(int sock) { 
			socket = sock;
		}

		// Receive an HTTP response message from the tracker server
		// parse according to: https://www.rfc-editor.org/rfc/rfc7230#section-3.
		void recv_http() { 
			
			char buffer[HTTP_HEADER_MAX_SIZE];
			int response_code = 0; 
			int payload_length = 0;

			// read the HTTP header, ensure that we got successful message
			// and successfully parsed payload length

			int recv_length = recv(socket, &buffer, HTTP_HEADER_MAX_SIZE, 0);  
			assert(recv_length > 0); // make sure socket didn't close on us 
			http_resp = std::string(buffer, recv_length);
			
			// Split on CRLF, parse the response code and content length
			// header fields
			std::vector<std::string> lines = split(http_resp, "\r\n"); 
			assert(!lines.empty()); 
			
			for(auto& line : lines) {
				std::vector<std::string> words = split(line, " ");

				if(words[0] == "HTTP/1.1 ") response_code = stoi(words[1]);
				else if(words[0] == "Content-Length: ") payload_length = stoi(words[1]);
			}
			
			// make sure we got the full message, and OK status
			bencoded_payload = lines.back();
			assert(response_code == HTTP_OK); 
			assert(payload_length > 0); 
			assert(bencoded_payload.length() == payload_length);
			
			parse_payload(bencoded_payload);
		}
		
		// bdecode the payload that we received from the tracker server, 
		// fill in response fields, and store all clients that we receive.
		void parse_payload(std::string payload) { 
			bencode::data data  = bencode::decode(payload);
			auto resp_dict = std::get<bencode::dict>(data); 

			// If we failed, other fields are not guaranteed to exist
			if(resp_dict.find("failure reason") != resp_dict.end()) {
				failure_reason = std::get<std::string>(resp_dict["failure reason"]); 
				std::cout << failure_reason << std::endl;
			}
			else {

				if(resp_dict.find("tracker id") != resp_dict.end()) {
					tracker_id = std::get<std::string>(resp_dict["tracker id"]); 
				}
				
				interval = std::get<long long>(resp_dict["interval"]); 
				num_seeders = std::get<long long>(resp_dict["complete"]); 
				num_leechers = std::get<long long>(resp_dict["incomplete"]); 

				// match peers onto its specified type using a visit
				std::visit([&] (auto && arg) {
					using T = std::decay_t <decltype(arg)>;
					
					// Integer peers? According to bittorrent protocol, this isn't possible.
					if constexpr (std::is_same_v<T, bencode::integer>) { std::cout << "peers mapped to int?" << std::endl; }

					// Map peers? According to bittorrent protocol, this isn't possible.
					else if constexpr (std::is_same_v<T, bencode::dict>) { std::cout << "peers mapped to map?" << std::endl; }

					// Binary peer model
					else if constexpr (std::is_same_v<T, bencode::string>) {
						
						// read 6 bytes at a time for each peer 
						std::stringstream ss(arg);
						char block[6]; 
						while(ss.read(block, 6)) { 
							
							uint32_t ip_addr; 
							uint16_t port; 

							// first 4 bytes are for ip address in network byte order; 
							// last 2 bytes are for port in network byte order;
							memcpy(&ip_addr, block, sizeof(uint32_t));
							memcpy(&port, block + sizeof(uint32_t), sizeof(uint16_t)); 
							
							ProtocolUtils::Peer peer(ip_addr, port); 
							peers.push_back(peer); 
						}
					}
					
					// Dictionary peer model
					else if constexpr (std::is_same_v<T, bencode::list>) {
						for(auto && entry : arg) {
							auto peer_dict = std::get<bencode::dict>(entry); 
							std::string peer_id = std::get<std::string>(peer_dict["peer id"]);
							std::string ip = std::get<std::string>(peer_dict["ip"]); 
							int port = std::get<long long>(peer_dict["port"]);
							
							ProtocolUtils::Peer peer(peer_id, ip, port); 
							peers.push_back(peer);
						}
					}
				}, resp_dict["peers"].base());
			}
		}
	};
}

#endif
