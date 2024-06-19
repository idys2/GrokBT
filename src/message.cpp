#include "message.h"

Messages::Buffer::Buffer(uint32_t length)
{
    bytes_read = 0;
    total_length = length + sizeof(length); // include the size of the length field in the buffer
    ptr = std::make_unique<uint8_t[]>(total_length);
}

Messages::Buffer::Buffer(uint8_t pstrlen)
{
    bytes_read = 0;
    total_length = 49 + pstrlen;
    ptr = std::make_unique<uint8_t[]>(total_length);
}

Messages::Handshake::Handshake() {}

Messages::Handshake::Handshake(uint8_t pstrlen, std::string pstr, std::string info_hash, std::string peer_id)
{
    this->pstrlen = pstrlen;
    this->pstr = pstr;
    this->info_hash = info_hash;
    this->peer_id = peer_id;
    memset(&reserved[0], 0, 8);
    total_length = 49 + pstrlen;
}

Messages::Buffer *Messages::Handshake::pack()
{

    Messages::Buffer *buffer = new Messages::Buffer(pstrlen);

    // for advancing the pointer when we read from the buffer
    int idx = 0;
    memcpy(buffer->ptr.get() + idx, &pstrlen, sizeof(pstrlen));
    idx += sizeof(pstrlen);

    memcpy(buffer->ptr.get() + idx, pstr.c_str(), pstrlen);
    idx += pstrlen;

    memcpy(buffer->ptr.get() + idx, &reserved[0], 8);
    idx += 8;

    memcpy(buffer->ptr.get() + idx, info_hash.c_str(), 20);
    idx += 20;

    memcpy(buffer->ptr.get() + idx, peer_id.c_str(), 20);

    buffer->total_length = total_length;
    return buffer;
}

void Messages::Handshake::unpack(Messages::Buffer *buf)
{

    int idx = 0;
    memcpy(&pstrlen, buf->ptr.get() + idx, sizeof(pstrlen));
    idx += sizeof(pstrlen);

    pstr = std::string((const char *)(buf->ptr.get() + idx), pstrlen);
    idx += pstrlen;

    memcpy(&reserved[0], buf->ptr.get() + idx, 8);
    idx += 8;

    info_hash = std::string((const char *)(buf->ptr.get() + idx), 20);
    idx += 20;

    peer_id = std::string((const char *)(buf->ptr.get() + idx), 20);
    idx += 20;

    total_length = 49 + pstrlen;
}

std::string Messages::Handshake::to_string()
{
    std::string ret = "Peer id: " + peer_id + "\n" + "Info hash: " + info_hash + "\n" + "pstr: " + pstr + "\n";
    return ret;
}

std::string Messages::Handshake::get_info_hash()
{
    return info_hash;
}

std::string Messages::Handshake::get_peer_id()
{
    return peer_id;
}