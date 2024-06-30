#ifndef FILE_H
#define FILE_H

#include <vector>
#include <string>
#include <queue>
#include <memory>
#include <fstream>

#include "message.h"
#include "hash.h"
#include "bencode_wrapper.hpp"

namespace File
{
	static sha1sum_ctx *ctx = sha1sum_create(NULL, 0);

	// A bitfield used in the bittorrent protocol. Wraps a vector of bytes.
	struct BitField
	{
		std::vector<uint8_t> bits; // the bitfield, stored as a byte vector
		uint32_t num_bits;		   // the size of the bitfield. This field is needed because some bits will be unused

		// Construct a bitfield by interpreting a buffer as a bitfield message
		BitField(Messages::Buffer *buff);

		// Initialize a bit field that can hold num_bits number of bits, all unflipped
		BitField(uint32_t num_bits);

		// check if the bit representing bit_index is true
		bool is_bit_set(uint32_t bit_index);

		// set the bit representing bit_index to be true
		void set_bit(uint32_t bit_index);

		// unset the bit representing bit_index to be true
		void unset_bit(uint32_t bit_index);

		// find the first bit_index where this bitfield has an unflipped bit, but the BitField other
		// has a flipped bit.
		// return the bit_index if found, or -1 if no such bit_index exists
		int first_match(BitField *other);

		// find the first bit_index in this bitfield that is not set to true
		// return this bit_index if found, or -1
		int first_unflipped();

		// see if all bits are flipped in this bitfield
		// return true if they are, false if not
		bool all_flipped();

		// pack this bitfield into a buffer
		// The returned buffer must be freed by the caller.
		Messages::Buffer *pack();

		// Print out this bitfield as a string
		std::string to_string();
	};

	// A block of the file we are torrenting
	// In the bittorrent protocol, requests/pieces are transacted via blocks. These are characterized by which piece they belong to
	// as well as their position (byte offset + length) within the piece.
	struct Block
	{
		uint32_t index;	 // the piece index for this block
		uint32_t begin;	 // the byte offset where this block begins
		uint32_t length; // the length of this block in bytes

		Block(uint32_t index, uint32_t begin, uint32_t length)
		{
			this->index = index;
			this->begin = begin;
			this->length = length;
		}

		std::string to_string()
		{
			return "index: " + std::to_string(index) + " begin: " + std::to_string(begin) + " length: " + std::to_string(length);
		}
	};

	// Because we transact in subsets of pieces (blocks), each piece holds a bitfield representing the blocks
	// that are within that piece. The piece struct manages these blocks.
	// note that all pieces are not guaranteed to be the same size. By extension, this means that each block will also
	// not necessarily be the same size
	struct Piece
	{
		static const long long block_size = 1 << 14; // the max size of each block in this piece

		std::string piece_hash; // 20 byte SHA1 hash for this piece
		uint32_t piece_index;	// the piece index of this piece

		std::unique_ptr<BitField> block_bitfield; // a bitfield tracking which blocks have been downloaded by the torrent
		std::unique_ptr<uint8_t[]> data;		  // pointer to a buffer that we write to when we download. Size piece_size.


		long long piece_size; // the max size of this piece
		uint32_t num_blocks;  // the number of blocks in this piece

		Piece(std::string piece_hash, uint32_t piece_index, long long size);

		// Return a vector of unfinished block structs using the block bitfield
		std::vector<Block> get_unfinished_blocks();
	};

	// A base torrent file. To be inherited by either a multi file torrent, or a single file torrent.
	// Contains information about piece length, number of pieces.
	class Torrent
	{
	protected:
		long long piece_length;	 // the length of each piece in a given torrent in bytes
		long long private_field; // field indicating if peers must show peer_id

		std::string pieces; // concatenation of 20 byte SHA1 hashes, with one per piece

		// construct a metadata class using a string buffer containing all metadata
		Torrent(std::string metainfo_buffer);
	};

	// A torrent consisting of a single file.
	// The torrent object will both
	// - keep track of all requests for blocks that have not been downloaded. This is so that we can quickly
	// determine the next requests that have to be made.
	// - keep track of all pieces that have been downloaded via a piece bitfield.
	class SingleFileTorrent : public Torrent
	{
	private:
		std::string name;	 // the name of the file being torrented
		long long length;	 // the length of the file in bytes
		uint32_t num_pieces; // the number of pieces in this torrent

		std::vector<Piece> piece_vec; // vector of pieces, indexed by piece indices
	public:
		std::unique_ptr<BitField> piece_bitfield; // bitfield of pieces. Used for fast intersection with peer bitfields
		std::queue<Block> block_queue;			  // queue of block requests that we are currently trying to make

		std::ofstream out_stream; 		// the file stream that this torrent should write out to
		SingleFileTorrent(std::string metainfo_buffer, std::string out_file);

		// Get all unfinished blocks from all unfinished piece vectors,
		// and place these into the block queue
		void update_block_queue();

		// Interpret a buffer as a piece message, then write its contents to the representing piece struct
		// and its data field. This function will not write unless the provided block stays within the piece
		// size bounds.
		void write_block(Messages::Buffer *buff);
	};

	// TODO: finish this
	class MultipleFileTorrent : public Torrent
	{
	private:
		std::string name; // the name of the directory to save files
	};
}

#endif