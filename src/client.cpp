#include "client.h"
#include <argparse/argparse.hpp>

std::string Client::unique_peer_id(std::string client_id) {
    assert(client_id.length() == 6);
    std::string peer_id = "-" + client_id + "-";
    std::string pid_str = std::to_string(getpid());  // process id as string 
    std::string timestamp_str = std::to_string(time(nullptr));  // timestamp as string
    std::string input = pid_str + timestamp_str;

    // we only use 20 characters at most for the peer ID
    char checksum[20]; 
    int error = sha1sum_finish(ctx, (const uint8_t *) input.c_str(), input.length(), (uint8_t *) checksum);
    assert(!error); 
    assert(sha1sum_reset(ctx) == 0);

    std::string unique_hash = std::string(checksum, 12); // 6 bytes from client_id, + 2 for guards. We take 12 bytes from the hash
    peer_id += unique_hash; 
    assert(peer_id.length() == 20);

    return peer_id;
}

std::string Client::read_info_dict_str(std::string metainfo_buffer) { 
    bencode::data data = bencode::decode(metainfo_buffer);
    auto metainfo_dict = std::get<bencode::dict>(data);
    auto info_dict = std::get<bencode::dict>(metainfo_dict["info"]);

    // match onto string
    std::string info_dict_str = bencode::encode(info_dict);
    return info_dict_str;
}

std::string Client::read_announce_url(std::string metainfo_buffer) { 
    bencode::data data = bencode::decode(metainfo_buffer); 
    auto metainfo_dict = std::get<bencode::dict>(data);     // a very messy type!
    return std::get<std::string>(metainfo_dict["announce"]);
}

std::string Client::read_metainfo(std::string filename) {
    std::ifstream stream(filename, std::ifstream::in);
    
    if(stream) {
        stream.seekg(0, stream.end);
        int length = stream.tellg();
        stream.seekg(0, stream.beg);

        char* buffer = new char[length];
        std::string ret;

        if(stream.read(buffer, length)) {
            ret.assign(buffer, length); 
            delete[] buffer; 
        };
        return ret;
    }
    else {
        std::cout << "file not found" << std::endl;
    }
    
    return "";
}

std::string Client::hash_info_dict_str(std::string info_dict_str) {
    char checksum[20]; 
    int error = sha1sum_finish(ctx, (const uint8_t *) info_dict_str.c_str(), info_dict_str.length(), (uint8_t *) checksum);
    assert(!error);
    assert(sha1sum_reset(ctx) == 0);

    return std::string(checksum, 20);
}   

int main(int argc, char* argv[]) {
    argparse::ArgumentParser program("client");

    std::string torrent_file;
    std::string client_id;
    int port;

    program.add_argument("-f").default_value("debian1.torrent").store_into(torrent_file);
    program.add_argument("-id").default_value("EZ6969").store_into(client_id);
    program.add_argument("-p").default_value(6881).store_into(port);
    try {
        program.parse_args(argc, argv);
    }
    catch (const std::exception& err) {
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        std::exit(1);
    }

    std::cout << torrent_file << std::endl;
    std::cout << client_id << std::endl;
    std::cout << port << std::endl;

    return 0;
}


