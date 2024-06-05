#ifndef FILE_H
#define FILE_H

#include <vector>
#include <string>

#include "hash.h"

namespace Torrent {

    struct BitField { 
        std::vector<uint8_t> bits;
        int num_bits;

        BitField();

        BitField(int b);

        BitField(uint8_t* data, int size);

        BitField(BitField &b);

        bool is_bit_set(int bit_index);

        void set_bit(int bit_index);
        
        int first_match(BitField other);

        int first_unflipped();

        bool all_flipped();
    };

    struct FilePiece {
        long long piece_byte_length; 	// the number of bytes for this piece, includes the case of trailing piece size
        long long bytes_down; 			// the number of bytes that we've downloaded for this piece
        int num_blocks;					// total number of blocks = ceil(piece_byte_length / block_size)

        uint32_t piece_index; 			// the index of this piece
        uint32_t block_size;		 	// the maximum number of bytes we can request in a single block
        std::string hash;				
        std::vector<char> block_buffer;		
        BitField block_bitfield; 		

        FilePiece(long long piece_length, uint32_t piece_index, std::string hash, uint32_t block_size);

        void download_block(int begin, int block_len, std::vector<uint8_t> block);

        bool is_done();

        bool verify_hash();

        void write_to_disk(std::ofstream stream);

        std::pair<uint32_t, uint32_t> get_next_request();

        int get_block_index(int byte_offset, int len);
    };

    struct File {
        long long piece_length;     // the length of each piece in the file
        long long length; 		    // the length of the file
        long long block_length;

        std::string file_name; 	
        std::vector<FilePiece> piece_vec;  
        BitField piece_bitfield;
        
        File();

        File(long long piece_length, std::string pieces, std::string fname, long long length, long long block_length);
    };


}




#endif