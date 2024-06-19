#include "client.h"
#include "tracker_protocol.h"

#define on_error(x) {std::cerr << x << ": " << strerror(errno) << std::endl; exit(1); }

/**
 * TODO: Test handshaking on a single peer
 */
int main() {

    // parse metafile and obtain announce url
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

	assert(!peers.empty());
    //assert(peers.size() == 1);

    // get info hash
    std::string metainfo_buffer = Metainfo::read_metainfo(torrent_file);
    std::string info_dict_str = Metainfo::read_info_dict_str(metainfo_buffer);
    std::string info_hash = Metainfo::hash_info_dict_str(info_dict_str);

    // create socket for peer and connect
    Peer::PeerClient peer = peers[0];

    std::cout << "Attempting to connect to: " << peer.to_string() << std::endl;

    int peer_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (peer_sock < 0) on_error("Failed to create socket");

    if (connect(peer_sock, (struct sockaddr *) &peer.sockaddr, sizeof(peer.sockaddr)) < 0) {
        on_error("Could not connect to peer");
    }

    // send client handshake
    Messages::Handshake client_handshake = Messages::Handshake(19, "BitTorrent protocol", info_hash, client_id);

    Messages::Buffer *buf = client_handshake.pack();
    assert(sendall(peer_sock, (buf->ptr).get(), &(buf->total_length)) >= 0);
    delete buf;

    // receive peer handshake
    int bytes_recv = 0;

    uint8_t pstrlen;
    bytes_recv += recv(peer_sock, &pstrlen, 1, 0);

    peer.buffer = new Messages::Buffer(pstrlen);
    (peer.buffer)->bytes_read += bytes_recv;

    bytes_recv += recv(peer_sock, (peer.buffer->ptr).get() + 1, 48 + pstrlen, 0);
    (peer.buffer)->bytes_read += bytes_recv;

    Messages::Handshake peer_handshake = Messages::Handshake();
    peer_handshake.unpack(peer.buffer);

    std::cout << client_handshake.to_string() << std::endl;
    std::cout << peer_handshake.to_string() << std::endl;
    //assert(client_handshake.info_hash == peer_handshake.info_hash);
}