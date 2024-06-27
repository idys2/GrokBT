#ifndef FILE_H
#define FILE_H

#include <vector>
#include <string>

#include "hash.h"

namespace File
{

    struct BitField
    {
        std::vector<uint8_t> bits;
        uint32_t num_bits;

        BitField(uint8_t *data, uint32_t size);

        bool is_bit_set(uint32_t bit_index);

        void set_bit(uint32_t bit_index);

        uint32_t first_match(BitField other);

        uint32_t first_unflipped();

        bool all_flipped();
    };

}

#endif