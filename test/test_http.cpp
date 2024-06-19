#include "client.h"
#include "tracker_protocol.h"

/**
 * Test connection to a tracker server
 */
int main() {
	std::string torrent_file = std::string(PROJECT_ROOT) + "/torrents/debian1.torrent";
	std::string client_id = "EZ6969";
	int port = 6881;

	// generate unique peer id
	std::string peer_id = Client::unique_peer_id(client_id);

	// connect to tracker
	TrackerProtocol::TrackerManager tracker(torrent_file, peer_id, port);
	tracker.send_http();
	tracker.recv_http();
	std::vector<Peer::PeerClient> peers = tracker.peers;

	// print list of peers
	assert(!peers.empty());
	for (auto peer : peers) std::cout << peer.to_string() << std::endl;

	return 0; 
}
