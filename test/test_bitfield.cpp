#include <iostream>
#include <cstring>
#include <string>
#include <thread>

#include <assert.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <assert.h>

#include "client.h"
#include "tracker_protocol.h"
#include "peer_wire_protocol.h"

#define TIMEOUT 120

const std::string torrent_file_path = "bigfile.torrent";
const std::string id = "EZ6969";
const int listen_port = 6881;
const int MAX_LISTEN_Q = 50;


int main() {
    ProtocolUtils::BitField bf = ProtocolUtils::BitField(63);
    assert(bf.num_bits == 63 && bf.bitfield.size() == 8);
    for (int i = 0; i < 8; i++) {
        assert(bf.bitfield[i] == 0);
    }

    for (int i = 0; i < 8; i++) {
        bf.set_bit(8*i + i);
    }
    std::cout << "FLIPPED BITS" << std::endl;
    for (int i = 0; i < 8; i++) {
        std::cout << "Byte " << i << " is " << std::bitset<8>(bf.bitfield[i]).to_string() << std::endl;
        //assert(bf.bitfield[i] == (128 >> i));
    }
    std::cout << "CORRECT BITS WERE FLIPPED" << std::endl;
    assert(!bf.all_flipped());
    assert(bf.first_unflipped() == 1);

    std::cout << "FINISHED!" << std::endl;
}