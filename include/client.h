#ifndef CLIENT_H
#define CLIENT_H

#include <string>
#include <fstream>
#include <stdio.h>

#include <assert.h>
#include <unistd.h>

#include "hash.h"
#include "bencode.hpp"

namespace Client {

	static sha1sum_ctx *ctx = sha1sum_create(NULL, 0);

	// get a unique peer id string given the client ID provided. This is done in the Azureus styling.
    std::string unique_peer_id(std::string client_id);

	// get the info dict string from a string buffer containing the metainfo file
	std::string read_info_dict_str(std::string metainfo_buffer);

	// read the announce url from a string buffer containing the metainfo file
	std::string read_announce_url(std::string metainfo_buffer);

	// read the metainfo and pack into a string buffer 
	std::string read_metainfo(std::string filename);

	// Compute the 20 byte SHA1 Hash of the bencoded info dictionary string
	// obtained from the metainfo file
	std::string hash_info_dict_str(std::string info_dict_str);
}

#endif