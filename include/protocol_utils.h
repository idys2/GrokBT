#ifndef _PROTOCOL_UTILS_H
#define _PROTOCOL_UTILS_H

#include <assert.h>
#include <unistd.h>
#include <sys/socket.h>

#include <fstream>
#include <cstring>
#include <string>

#include "hash.h"
#include "bencode.hpp"

// Utility functions that are shared between both the 
// Tracker HTTP Protocol and the Peer Wire Protocol to be used by clients
namespace ProtocolUtils { 

	const std::string TORRENT_FOLDER_PATH = "//home//student//fp//torrents//";
	sha1sum_ctx *ctx = sha1sum_create(NULL, 0); 

	struct single_info {
		long long piece_length; 
		std::string pieces; 
		std::string name; 
		long long length; // total file size
	};
	
	// From Beej's guide: sendall bytes in buffer via TCP
	int sendall(int s, const char *buf, int *len) {
		int total = 0;        // how many bytes we've sent
		int bytesleft = *len; // how many we have left to send
		int n;

		while(total < *len) {
			n = send(s, buf+total, bytesleft, 0);
			if (n == -1) { break; }
			total += n;
			bytesleft -= n;
		}

		*len = total; // return number actually sent here

		return n==-1?-1:0; // return -1 on failure, 0 on success
	}

	// Compute the 20 byte SHA1 Hash of the bencoded info dictionary string
	// obtained from the metainfo file
	std::string hash_info_dict_str(std::string info_dict_str) {
		char checksum[20]; 
		int error = sha1sum_finish(ctx, (const uint8_t *) info_dict_str.c_str(), info_dict_str.length(), (uint8_t *) checksum);
		assert(!error);
		sha1sum_reset(ctx);

		return std::string(checksum, 20);
	}
	
	// Compute the 20 byte peer id, using the Azureus style
	// the user provides the client_id, consisting of 
	// 2 characters, following by 4 integers for version number
	// the function produces a unique hash and appends to client id
	std::string unique_peer_id(std::string client_id) {
		assert(client_id.length() == 6);
		std::string peer_id = "-" + client_id + "-";
		std::string pid_str = std::to_string(getpid());  // process id as string 
		std::string timestamp_str = std::to_string(time(nullptr));  // timestamp as string
		std::string input = pid_str + timestamp_str;

		char checksum[20]; 
		int error = sha1sum_finish(ctx, (const uint8_t *) input.c_str(), input.length(), (uint8_t *) checksum);
		assert(!error); 
		sha1sum_reset(ctx);

		std::string unique_hash = std::string(checksum, 12); // 6 bytes from client_id, + 2 for guards. We take 12 bytes from the hash
		peer_id += unique_hash; 
		assert(peer_id.length() == 20);

		return peer_id;
	}

	// Parse metainfo file into byte buffer
	// the caller must free the buffer returned
	std::string read_metainfo(std::string filename) {
		std::string filepath = TORRENT_FOLDER_PATH + filename;
		std::ifstream stream(filepath, std::ifstream::in);
		
		if(stream) {
			stream.seekg(0, stream.end);
			int length = stream.tellg();
			stream.seekg(0, stream.beg);

			char* buffer = new char[length];
			std::string ret;

			if(stream.read(buffer, length)) {
				ret.assign(buffer, length); 
				delete[] buffer; 
			};

			return ret;
		}

		else {
			std::cout << "file not found" << std::endl;
		}
		
		return "";
	}

	// Read announce url from metainfo file
	// the metainfo_buffer contains a std::string packed with the bencoded data
	std::string read_announce_url(std::string metainfo_buffer) {
		bencode::data data = bencode::decode(metainfo_buffer); 
		auto metainfo_dict = std::get<bencode::dict>(data); 
		return std::get<std::string>(metainfo_dict["announce"]);
	}

	// Read the info dictionary from metainfo file 
	// the metainfo_buffer contains a std::string packed with the bencoded data
	single_info read_info_dict(std::string metainfo_buffer) {	
		
		// read from metainfo file
		bencode::data data = bencode::decode(metainfo_buffer);
		auto metainfo_dict = std::get<bencode::dict>(data);
		auto info_dict = std::get<bencode::dict>(metainfo_dict["info"]);

		// set struct fields
		single_info ret;
		ret.piece_length = std::get<long long>(info_dict["piece length"]);
		ret.pieces = std::get<std::string>(info_dict["pieces"]);
		ret.name = std::get<std::string>(info_dict["name"]);
		ret.length = std::get<long long>(info_dict["length"]);
		return ret;
	}

	// gets std::string representation of the value of info key (returns bencoded dictionary)
	std::string read_info_dict_str(std::string metainfo_buffer) { 
		// read from metainfo file
		bencode::data data = bencode::decode(metainfo_buffer);
		auto metainfo_dict = std::get<bencode::dict>(data);
		auto info_dict = std::get<bencode::dict>(metainfo_dict["info"]);

		// get std::string representation of info dict which is type bencode::dict
		std::string info_dict_str = bencode::encode(info_dict);
		return info_dict_str;
	}
	
}

#endif
