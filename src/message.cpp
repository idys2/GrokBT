#include "message.h"

Messages::Buffer::Buffer() {
    bytes_read = 0;
    total_len = 0;  // include the size of the length field in the buffer
    ptr = nullptr;
}

Messages::Buffer::Buffer(uint32_t len) {
    bytes_read = 0;
    total_len = len + sizeof(len);  // include the size of the length field in the buffer
    ptr = std::make_unique<uint8_t[]>(total_len);
}

Messages::Buffer::Buffer(Buffer& b) { 
    bytes_read = b.bytes_read;
    total_len = b.total_len;
    ptr = std::move(b.ptr);
}

Messages::Buffer& Messages::Buffer::operator=(const Buffer& b) {
    bytes_read = b.bytes_read;
    total_len = b.total_len;  // include the size of the length field in the buffer
    ptr = std::move(b.ptr);

    return *this;
}


Messages::Message::Message() {}

Messages::Handshake::Handshake() {}

Messages::Buffer Messages::Handshake::pack() {
    Messages::Buffer buffer = Messages::Buffer(pstrlen);

    int idx = 0;
    memcpy(buffer.ptr + idx, &pstrlen, sizeof(pstrlen));
    idx += sizeof(pstrlen);
    
    memcpy(buffer.ptr + idx, pstr.c_str(), pstrlen);
    idx += pstrlen; 

    memcpy(buffer.ptr + idx, &reserved[0], 8);
    idx += 8; 

    memcpy(buffer.ptr + idx, info_hash.c_str(), 20);
    idx += 20;
    
    memcpy(buffer.ptr + idx, peer_id.c_str(), 20);
}

void Messages::Handshake::unpack(Messages::Buffer buf) {
    int idx = 0;
    memcpy(pstrlen, &buf.ptr[idx], sizeof(pstrlen));
    idx += sizeof(pstrlen);

    pstr = std::string(&buf.ptr[idx], pstrlen);
    idx += pstrlen;
    
    memcpy(reserved, &buf.ptr[idx], 8);
    idx += 8;

    info_hash = std::string(&buf.ptr[idx], 20);
    idx += 20;

    peer_id = std::string(&buf.ptr[idx], 20);
}

std::string Messages::Handshake::to_string() {
    return std::to_string(pstrlen) + " " + pstr + " " + info_hash + " " + peer_id;
}

