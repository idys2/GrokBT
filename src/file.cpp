#include "file.hpp"

#include <iostream>
namespace File
{

    BitField::BitField(Messages::Buffer *buff, uint32_t num_bits)
    {
        // read len and id fields from the buffer
        uint32_t len;
        uint8_t id;
        uint32_t num_bytes;

        int idx = 0;
        memcpy(&len, buff->ptr.get() + idx, sizeof(len));
        idx += sizeof(len);

        memcpy(&id, buff->ptr.get() + idx, sizeof(id));
        idx += sizeof(id);

        // need to pass in number of bits, because we only get the number of bytes from the bitfield message
        this->num_bits = num_bits;
        num_bytes = len - sizeof(id);
        bits = std::vector<uint8_t>(num_bytes, 0);

        // read bitfield data from after the len and id fields
        for (int i = 0; i < num_bytes; i++)
        {
            bits[i] = (buff->ptr.get() + idx)[i];
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

    void BitField::unset_bit(uint32_t bit_index)
    {
        uint32_t byte_index = bit_index / 8; // the index in the vector for this piece index
        uint32_t offset = bit_index % 8;     // the bit we are interested in

        // bitshift the correct bit
        bits[byte_index] &= 0 << (7 - offset);
    }

    int BitField::first_match(BitField *other)
    {
        for (int i = 0; i < num_bits; i++)
        {
            if (!is_bit_set(i) && other->is_bit_set(i))
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

    Messages::Buffer *BitField::pack()
    {
        uint32_t len = bits.size() + sizeof(Messages::BITFIELD_ID);
        Messages::Buffer *buff = new Messages::Buffer((uint32_t) (len + sizeof(len)));

        int idx = 0;

        // pack len and idx
        uint32_t big_endian_len = htonl(len);
        memcpy(buff->ptr.get() + idx, &big_endian_len, sizeof(big_endian_len));
        idx += sizeof(big_endian_len);

        memcpy(buff->ptr.get() + idx, &Messages::BITFIELD_ID, sizeof(Messages::BITFIELD_ID));
        idx += sizeof(Messages::BITFIELD_ID);

        // pack bitfield
        memcpy(buff->ptr.get() + idx, bits.data(), bits.size());

        return buff;
    }

    std::string BitField::to_string()
    {
        std::string ret = "";

        for(int i = 0; i < num_bits; i++) {
            if(is_bit_set(i)) {
                ret += "index " + std::to_string(i) + ": 1\n";
            }
            else {
                ret += "index " + std::to_string(i) + ": 0\n";
            }
        }

        return ret;
    }

    // Declare static vars
    Piece::Piece(std::string piece_hash, uint32_t piece_index, long long size)
    {
        this->piece_hash = piece_hash;
        this->piece_index = piece_index;

        piece_size = size;
        num_blocks = std::ceil((double)piece_size / Piece::block_size);

        data = std::make_unique<uint8_t[]>(size); // the piece will hold exactly size bytes
        block_bitfield = std::make_unique<BitField>(num_blocks);
    }

    std::vector<Block> Piece::get_unfinished_blocks()
    {
        std::vector<Block> blocks;

        // for all full sized blocks, create block_size blocks
        for (int i = 0; i < Piece::num_blocks - 1; i++)
        {
            if(!block_bitfield->is_bit_set(i)) {
                blocks.push_back(Block(piece_index, i * Piece::block_size, Piece::block_size));
            }
        }

        // the last block might be not be a full sized block
        // create a block that is sized with however many bytes are left
        if(!block_bitfield->is_bit_set(num_blocks - 1)) {
            uint32_t bytes_left = piece_size - (num_blocks - 1) * Piece::block_size;
            uint32_t block_offset = (num_blocks - 1) * Piece::block_size;
            blocks.push_back(Block(piece_index, block_offset, bytes_left));
        }

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
        out_stream = std::ofstream(name);

        num_pieces = pieces.length() / 20; // pieces is a concatenation of 20 byte hashes, so divide by 20 to number of pieces
        piece_vec = std::vector<Piece>();
        piece_bitfield = std::make_unique<BitField>(num_pieces);

        // init tracking stats
        downloaded = 0;
        uploaded = 0;

        for (int i = 0; i < num_pieces - 1; i++)
        {
            std::string piece_hash = pieces.substr(i * 20, 20);      // read 20 bytes at a time
            piece_vec.push_back(Piece(piece_hash, i, piece_length)); // create Piece objects
        }

        // last piece may not be a full piece size
        std::string last_piece_hash = pieces.substr((num_pieces - 1) * 20, 20);
        uint32_t bytes_left = length - (num_pieces - 1) * piece_length;
        piece_vec.push_back(Piece(last_piece_hash, num_pieces - 1, bytes_left));

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

    void SingleFileTorrent::write_block(Messages::Buffer *buff)
    {
        uint32_t len;   // the length of the message
        uint8_t id;     // the id of the message. note that this must be the piece message id, because we checked prior to recv
        uint32_t index; // the index of the piece
        uint32_t begin; // the byte offset where this block begins

        int idx = 0;
        // read len
        memcpy(&len, buff->ptr.get() + idx, sizeof(len));
        idx += sizeof(len);

        // read id
        memcpy(&id, buff->ptr.get() + idx, sizeof(id));
        idx += sizeof(id);

        // read index
        memcpy(&index, buff->ptr.get() + idx, sizeof(index));
        idx += sizeof(index);

        // read begin
        memcpy(&begin, buff->ptr.get() + idx, sizeof(begin));
        idx += sizeof(begin);

        // convert to little endian
        index = ntohl(index);
        begin = ntohl(begin);

        std::cout << "got block: piece index " << index << " begin: " << begin << std::endl;

        // check that the piece conforms to a block that we requested
        uint32_t data_len = len - sizeof(index) - sizeof(begin) - sizeof(id);
        bool within_bounds = begin + data_len <= piece_vec[index].piece_size;
        if (within_bounds && piece_vec[index].data != nullptr)
        {
            uint32_t block_index = begin / Piece::block_size;                             // the index of the block that we are writing
            memcpy(piece_vec[index].data.get() + begin, buff->ptr.get() + idx, data_len); // write the block to the piece's buffer
            piece_vec[index].block_bitfield->set_bit(block_index);

            // if the piece is now finished, check the hash of the piece
            if (piece_vec[index].block_bitfield->all_flipped())
            {
                std::string piece_data = std::string((const char *) piece_vec[index].data.get(), piece_vec[index].piece_size);
                std::string down_piece_hash = Hash::truncated_sha1_hash(piece_data, 20);
                if (down_piece_hash != piece_vec[index].piece_hash)
                {
                    std::cout << "piece hash did not match" << std::endl;
                    // unflip all bits
                    for (int i = 0; i < piece_vec[index].block_bitfield->num_bits; i++)
                    {
                        piece_vec[index].block_bitfield->unset_bit(i);
                    }
                }

                // if piece hash matches, then we can just write this piece to out
                else
                {
                    downloaded += piece_vec[index].piece_size;
                    piece_bitfield->set_bit(index);
                    std::cout << "piece hash matched, writing to out" << std::endl;

                    // calculate the byte that this piece starts at
                    uint32_t start_byte = index * piece_length;
                    out_stream.seekp(start_byte, std::ios::beg);
                    out_stream.write(reinterpret_cast<char *>(piece_vec[index].data.get()), piece_vec[index].piece_size);

                    // free the piece data from memory
                }
            }
        }

        else
        {
            std::cout << "invalid block" << std::endl;
        }
    }


    Messages::Buffer *SingleFileTorrent::get_piece(uint32_t index, uint32_t begin, uint32_t length)
    {
        uint32_t len = length + sizeof(index) + sizeof(begin) + sizeof(Messages::PIECE_ID);
        Messages::Buffer *buff = new Messages::Buffer((uint32_t) (len + sizeof(len)));

        int idx = 0;

        // pack len and idx
        uint32_t big_endian_len = htonl(len);
        memcpy(buff->ptr.get() + idx, &big_endian_len, sizeof(big_endian_len));
        idx += sizeof(big_endian_len);

        memcpy(buff->ptr.get() + idx, &Messages::PIECE_ID, sizeof(Messages::PIECE_ID));
        idx += sizeof(Messages::PIECE_ID);

        // pack index and begin, note that these must be in network endian
        uint32_t big_endian_index = htonl(index);
        memcpy(buff->ptr.get() + idx, &big_endian_index, sizeof(big_endian_index));
        idx += sizeof(big_endian_index);

        uint32_t big_endian_begin = htonl(begin);
        memcpy(buff->ptr.get() + idx, &big_endian_begin, sizeof(big_endian_begin));
        idx += sizeof(big_endian_begin);    

        memcpy(buff->ptr.get() + idx, piece_vec[index].data.get() + begin, length);

        return buff;
    }
}