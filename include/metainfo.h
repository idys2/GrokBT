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

	class TorrentMetadata
	{
	public:
		long long piece_length;	 // the length of each piece in a given torrent
		std::string pieces;		 // concatenation of 20 byte SHA1 hashes, with one per piece
		long long private_field; // field indicating if peers must show peer_id

		// construct a metadata class using a string buffer containing all metadata
		TorrentMetadata(std::string metainfo_buffer);
	};

	class SingleFileTorrentMetadata : public TorrentMetadata
	{
	public:
		std::string name; // the name of the file being torrented
		long long length; // the length of the file in bytes

		SingleFileTorrentMetadata(std::string metainfo_buffer);
	};

	// TODO: finish this
	class MultipleFileTorrentMetadata : public TorrentMetadata
	{
	public:
		std::string name; // the name of the directory to save files
	};

	static sha1sum_ctx *ctx = sha1sum_create(NULL, 0);

	// get the info dict string from a string buffer containing the metainfo file
	std::string read_info_dict_str(std::string metainfo_buffer);

	// read the announce url from a string buffer containing the metainfo file
	std::string read_announce_url(std::string metainfo_buffer);

	// read the metainfo and pack into a string buffer
	std::string read_metainfo_to_buffer(std::string filename);

	// Compute the 20 byte SHA1 Hash of the bencoded info dictionary string
	// obtained from the metainfo file
	std::string hash_info_dict_str(std::string info_dict_str);
}

#endif