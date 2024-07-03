#ifndef METAINFO_H
#define METAINFO_H
#include <string>
#include <fstream>
#include <stdio.h>

#include <assert.h>
#include <unistd.h>

#include "hash.h"
#include "bencode_wrapper.hpp"

namespace Metainfo
{
	// get the info dict string from a string buffer containing the metainfo file
	std::string read_info_dict_str(std::string metainfo_buffer);

	// read the announce url from a string buffer containing the metainfo file
	std::string read_announce_url(std::string metainfo_buffer);

	// read the metainfo and pack into a string buffer
	std::string read_metainfo_to_buffer(std::string filename);
}

#endif