#include "file.h"

namespace File
{

    BitField::BitField(uint8_t *data, uint32_t num_bits)
    {
        this->num_bits = num_bits;
        uint32_t len = std::ceil((double)num_bits / 8.0); // we can have extra trailing bits at the end
        bits = std::vector<uint8_t>(len, 0);
        for (int i = 0; i < len; i++)
        {
            bits[i] = data[i];
        }
    }

    BitField::BitField(uint32_t num_bits)
    {
        this->num_bits = num_bits;
        uint32_t len = std::ceil((double)num_bits / 8.0); // we can have extra trailing bits at the end
        bits = std::vector<uint8_t>(len, 0);
    }

    bool BitField::is_bit_set(uint32_t bit_index)
    {
        uint32_t byte_index = bit_index / 8; // the index in the vector for this piece index
        uint32_t offset = bit_index % 8;     // the bit we are interested in
        return ((bits[byte_index] >> (7 - offset)) & 1) != 0;
    }

    void BitField::set_bit(uint32_t bit_index)
    {
        uint32_t byte_index = bit_index / 8; // the index in the vector for this piece index
        uint32_t offset = bit_index % 8;     // the bit we are interested in

        // bitshift the correct bit
        bits[byte_index] |= 1 << (7 - offset);
    }

    int BitField::first_match(BitField other)
    {
        for (int i = 0; i < num_bits; i++)
        {
            if (!is_bit_set(i) && other.is_bit_set(i))
            {
                return i;
            }
        }
        return -1;
    }

    int BitField::first_unflipped()
    {
        for (int i = 0; i < num_bits; i++)
        {
            if (!is_bit_set(i))
            {
                return i;
            }
        }
        return -1;
    }

    bool BitField::all_flipped()
    {
        for (int i = 0; i < num_bits; i++)
        {
            if (!is_bit_set(i))
            {
                return false;
            }
        }
        return true;
    }

    // Declare static vars
    long long Piece::piece_size;
    long long Piece::block_size;
    uint32_t Piece::num_blocks;

    Piece::Piece(std::string piece_hash, uint32_t piece_index, long long size)
    {
        this->piece_hash = piece_hash;
        this->piece_index = piece_index;

        piece_size = size;
        block_size = 1 << 14;
        num_blocks = std::ceil((double)piece_size / block_size);

        data = std::make_unique<uint8_t[]>(size); // the piece will hold exactly size bytes
        block_bitfield = std::make_unique<BitField>(num_blocks);
    }

    std::vector<Block> Piece::get_unfinished_blocks()
    {
        std::vector<Block> blocks;

        // for all full sized blocks, create block_size blocks
        for (int i = 0; i < Piece::num_blocks - 1; i++)
        {
            blocks.push_back(Block(piece_index, i * Piece::block_size, Piece::block_size));
        }

        // the last block might be not be a full sized block
        // create a block that is sized with however many bytes are left
        uint32_t bytes_left = Piece::piece_size - (Piece::num_blocks - 1) * Piece::block_size;
        uint32_t block_offset = (Piece::num_blocks - 1) * Piece::block_size;
        blocks.push_back(Block(piece_index, block_offset, bytes_left));

        return blocks;
    }

    Torrent::Torrent(std::string metainfo_buffer)
    {
        bencode::data data = bencode::decode(metainfo_buffer);
        auto metainfo_dict = std::get<bencode::dict>(data);
        auto info_dict = std::get<bencode::dict>(metainfo_dict["info"]);

        piece_length = std::get<long long>(info_dict["piece length"]);
        pieces = std::get<std::string>(info_dict["pieces"]);
    }

    SingleFileTorrent::SingleFileTorrent(std::string metainfo_buffer) : Torrent(metainfo_buffer)
    {
        bencode::data data = bencode::decode(metainfo_buffer);
        auto metainfo_dict = std::get<bencode::dict>(data);
        auto info_dict = std::get<bencode::dict>(metainfo_dict["info"]);

        name = std::get<std::string>(info_dict["name"]);
        length = std::get<long long>(info_dict["length"]);

        num_pieces = pieces.length() / 20; // pieces is a concatenation of 20 byte hashes, so divide by 20 to number of pieces
        piece_vec = std::vector<Piece>();
        piece_bitfield = std::make_unique<BitField>(num_pieces);

        for (int i = 0; i < num_pieces; i++)
        {
            std::string piece_hash = pieces.substr(i * 20, 20);      // read 20 bytes at a time
            piece_vec.push_back(Piece(piece_hash, i, piece_length)); // create Piece objects
        }

        update_block_queue();
    }

    void SingleFileTorrent::update_block_queue()
    {
        for (int i = 0; i < num_pieces; i++)
        {
            std::vector<Block> blocks = piece_vec[i].get_unfinished_blocks();
            for (Block &b : blocks)
            {
                block_queue.push(b);
            }
        }
    }

}