#include "message.h"

namespace Messages
{   
    // interpret data stored in the buffer as a bitfield message
    // returns a bitfield struct as defined in file.h
    File::BitField *parseBitField(Buffer *buff)
    {
        uint32_t len;
        uint8_t id;

        int idx = 0;
        memcpy(&len, buff->ptr.get() + idx, sizeof(len));
        idx += sizeof(len);

        memcpy(&id, buff->ptr.get() + idx, sizeof(id));
        idx += sizeof(id);

        return new File::BitField(buff->ptr.get(), len - sizeof(id));
    }

    // Pack a bitfield into a buffer
    Buffer bitFieldToBuffer(File::BitField bitfield)
    {
        uint32_t len = bitfield.num_bits + sizeof(BITFIELD_ID);
        Buffer b = Buffer(len + 1);

        int idx = 0;
        // pack len and idx
        memcpy(b.ptr.get() + idx, &len, sizeof(len));
        idx += sizeof(len);

        memcpy(b.ptr.get() + idx, &BITFIELD_ID, sizeof(BITFIELD_ID));
        idx += sizeof(BITFIELD_ID);

        // pack bitfield
        memcpy(b.ptr.get() + idx, bitfield.bits.data(), sizeof(bitfield.num_bits));

        return b;
    }
}
