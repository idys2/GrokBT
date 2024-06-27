#include "metainfo.h"

namespace Metainfo
{

    std::string read_info_dict_str(std::string metainfo_buffer)
    {
        bencode::data data = bencode::decode(metainfo_buffer);
        auto metainfo_dict = std::get<bencode::dict>(data);
        auto info_dict = std::get<bencode::dict>(metainfo_dict["info"]);

        // match onto string
        std::string info_dict_str = bencode::encode(info_dict);
        return info_dict_str;
    }

    std::string read_announce_url(std::string metainfo_buffer)
    {
        bencode::data data = bencode::decode(metainfo_buffer);
        auto metainfo_dict = std::get<bencode::dict>(data); // a very messy type!
        return std::get<std::string>(metainfo_dict["announce"]);
    }

    std::string read_metainfo_to_buffer(std::string filename)
    {
        std::ifstream stream(filename, std::ifstream::in);

        if (stream)
        {
            stream.seekg(0, stream.end);
            int length = stream.tellg();
            stream.seekg(0, stream.beg);

            char *buffer = new char[length];
            std::string ret;

            if (stream.read(buffer, length))
            {
                ret.assign(buffer, length);
                delete[] buffer;
            };
            return ret;
        }
        else
        {
            std::cout << "file not found" << std::endl;
        }

        return "";
    }

    std::string hash_info_dict_str(std::string info_dict_str)
    {
        char checksum[20];
        int error = sha1sum_finish(ctx, (const uint8_t *)info_dict_str.c_str(), info_dict_str.length(), (uint8_t *)checksum);
        assert(!error);
        assert(sha1sum_reset(ctx) == 0);

        return std::string(checksum, 20);
    }

    TorrentMetadata::TorrentMetadata(std::string metainfo_buffer)
    {
        bencode::data data = bencode::decode(metainfo_buffer);
        auto metainfo_dict = std::get<bencode::dict>(data);              // metainfo as a bencode dictionary
        auto info_dict = std::get<bencode::dict>(metainfo_dict["info"]); // info dictionary as a bencode dictionary

        piece_length = std::get<long long>(info_dict["piece length"]);
        pieces = std::get<std::string>(info_dict["pieces"]);
    }

    SingleFileTorrentMetadata::SingleFileTorrentMetadata(std::string metainfo_buffer) : TorrentMetadata(metainfo_buffer)
    {
        bencode::data data = bencode::decode(metainfo_buffer);
        auto metainfo_dict = std::get<bencode::dict>(data);              // metainfo as a bencode dictionary
        auto info_dict = std::get<bencode::dict>(metainfo_dict["info"]); // info dictionary as a bencode dictionary

        name = std::get<std::string>(info_dict["name"]);
        length = std::get<long long>(info_dict["length"]);
    }

}