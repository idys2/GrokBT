#ifndef CLIENT_H
#define CLIENT_H

#include <string>
#include <assert.h>

#include "hash.h"
#include "bencode.hpp"

namespace Client {

	const std::string id = "EZ6969";
	const int listen_port = 6881;

    sha1sum_ctx *ctx = sha1sum_create(NULL, 0);

    std::string unique_peer_id(std::string client_id);

	std::string get_info_dict_str(std::string metainfo_buffer);

	std::string read_announce_url(std::string metainfo_buffer);

	std::string read_metainfo(std::string filename);
}

#endif