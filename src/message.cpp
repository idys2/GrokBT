#include "message.h"

namespace Messages
{

    Handshake::Handshake(Buffer *buff) : Message(buff)
    {
        int idx = 0;
        // read the pstrlen field
        memcpy(&pstrlen, buff->ptr.get() + idx, sizeof(pstrlen));
        idx += sizeof(pstrlen);

        // read the pstr field
        pstr = std::string((const char *)(buff->ptr.get() + idx), pstrlen);
        idx += pstrlen;

        // read the reserved field
        memcpy(&reserved[0], buff->ptr.get() + idx, 8);
        idx += 8;

        // read the info hash field
        info_hash = std::string((const char *)(buff->ptr.get() + idx), 20);
        idx += 20;

        // read the peer_id field
        peer_id = std::string((const char *)(buff->ptr.get() + idx), 20);
        idx += 20;
    }

    void Handshake::pack(Buffer *buffer)
    {
        // for advancing the pointer when we write to the buffer
        int idx = 0;

        // pack the pstrlen first
        memcpy(buffer->ptr.get() + idx, &pstrlen, sizeof(pstrlen));
        idx += sizeof(pstrlen);

        // pack the pstr
        memcpy(buffer->ptr.get() + idx, pstr.c_str(), pstrlen);
        idx += pstrlen;

        // pack the reserved bytes
        memcpy(buffer->ptr.get() + idx, &reserved[0], 8);
        idx += 8;

        // pack the info hash
        memcpy(buffer->ptr.get() + idx, info_hash.c_str(), 20);
        idx += 20;

        // pack the peer id
        memcpy(buffer->ptr.get() + idx, peer_id.c_str(), 20);
    }
}
