#include "metainfo.hpp"

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
    
}