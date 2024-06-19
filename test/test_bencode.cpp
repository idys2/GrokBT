#include "metainfo.h"

/**
 * Tests bencode on a torrent (metainfo) file
 */
int main() {

	// get the ip address of the tracker
	std::string path = std::string(PROJECT_ROOT) + "/torrents";
	std::string metainfo = Metainfo::read_metainfo(path + "/debian1.torrent");
	std::string url = Metainfo::read_announce_url(metainfo);
	std::cout << "announce url: " << url << std::endl;

	// read info dictionary
	std::string ret = Metainfo::read_info_dict_str(metainfo);
	assert(ret != "");
	return 0; 
}
