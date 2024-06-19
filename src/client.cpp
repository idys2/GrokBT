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
        char checksum[20];
        int error = sha1sum_finish(ctx, (const uint8_t *)input.c_str(), input.length(), (uint8_t *)checksum);
        assert(!error);
        assert(sha1sum_reset(ctx) == 0);

        std::string unique_hash = std::string(checksum, 12); // 6 bytes from client_id, + 2 for guards. We take 12 bytes from the hash
        peer_id += unique_hash;
        assert(peer_id.length() == 20);

        return peer_id;
    }
}
