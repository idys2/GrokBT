#include <iostream>
#include <cstring>
#include <assert.h>

#include "protocol_utils.h"

int main() {

	// get the ip address of the tracker
	std::string metainfo = ProtocolUtils::read_metainfo("debian1.torrent");
	std::string url = ProtocolUtils::read_announce_url(metainfo);	
	std::cout << "announce url: " << url << std::endl;

	// read info dictionary
	ProtocolUtils::single_info ret = ProtocolUtils::read_info_dict(metainfo); 

	std::cout << "piece length: " << ret.piece_length << std::endl; 
	// this works but you probably dont want to print a bunch of garbage
	//std::cout << ret.pieces << std::endll; 
	std::cout << "name: " << ret.name << std::endl; 
	std::cout << "file length: " << ret.length << std::endl;

	std::string info_dict_as_string = ProtocolUtils::read_info_dict_str(metainfo);
	// std::cout << "bencoded info dict: " << info_dict_as_string << std::endll;

	return 0; 
}
