#include "client.h"

namespace Client
{
    std::string unique_peer_id(std::string client_id)
    {
        assert(client_id.length() == 6);
        std::string peer_id = "-" + client_id + "-";
        std::string pid_str = std::to_string(getpid());            // process id as string
        std::string timestamp_str = std::to_string(time(nullptr)); // timestamp as string
        std::string input = pid_str + timestamp_str;

        // we only use 20 characters at most for the peer ID
        // only 12 of the characters must be unique
        std::string unique_hash = Hash::truncated_sha1_hash(input, 12);
        peer_id += unique_hash;
        assert(peer_id.length() == 20);

        return peer_id;
    }
}
