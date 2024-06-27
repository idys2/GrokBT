#include "file.h"

namespace File {
    BitField::BitField(uint8_t* data, uint32_t size) { 
        num_bits = size;
        uint32_t len = std::ceil((double) num_bits / 8.0);	 // we can have extra trailing bits at the end				
        bits = std::vector<uint8_t>(len, 0);
        for(int i = 0; i < len; i++) {
            bits[i] = data[i];
        }
    } 

    bool BitField::is_bit_set(uint32_t bit_index) {
        uint32_t byte_index = bit_index / 8;	// the index in the vector for this piece index
        uint32_t offset = bit_index % 8;		// the bit we are interested in
        return ((bits[byte_index] >> (7 - offset)) & 1) != 0; 
    }

    void BitField::set_bit(uint32_t bit_index) {
        uint32_t byte_index = bit_index / 8;	// the index in the vector for this piece index
        uint32_t offset = bit_index % 8;       // the bit we are interested in

        // bitshift the correct bit
        bits[byte_index] |= 1 << (7 - offset);
    }

    uint32_t BitField::first_match(BitField other) { 
        for(uint32_t i = 0; i < num_bits; i++) {
            if(!is_bit_set(i) && other.is_bit_set(i)) {
                return i;
            }
        }
        return -1; 
    }

    uint32_t BitField::first_unflipped() {
        for(uint32_t i = 0; i < num_bits; i++) {
            if(!is_bit_set(i)) { 
                return i;
            }
        }
        return -1;
    }

    bool BitField::all_flipped() {
        for(int i = 0; i < num_bits; i++) {
            if (!is_bit_set(i)) {
                return false;
            }
        }
        return true;
    }

}