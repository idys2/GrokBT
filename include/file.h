#ifndef FILE_H
#define FILE_H

#include <vector>
#include <string>
#include <queue>
#include <memory>
#include <cmath>

#include "hash.h"
#include "bencode_wrapper.hpp"

namespace File
{

	// A bitfield used in the bittorrent protocol. Wraps a vector.
	struct BitField
	{
		std::vector<uint8_t> bits; // the bitfield, stored as a byte vector
		uint32_t num_bits;		   // the size of the bitfield. This field is needed because some bits will be unused

		// Construct a bitfield by reading size bytes starting from a pointer
		// This is used with Message Buffers so that we can read off the wire from bitfield messages
		BitField(uint8_t *data, uint32_t num_bits);

		// Initialize a bit field that can hold num_bits number of bits, all unflipped
		BitField(uint32_t num_bits);

		// check if the bit representing bit_index is true
		bool is_bit_set(uint32_t bit_index);

		// set the bit representing bit_index to be true
		void set_bit(uint32_t bit_index);

		// find the first bit_index where this bitfield has an unflipped bit, but the BitField other 
		// has a flipped bit.
		// return the bit_index if found, or -1 if no such bit_index exists
		int first_match(BitField other);

		// find the first bit_index in this bitfield that is not set to true
		// return this bit_index if found, or -1
		int first_unflipped();

		// see if all bits are flipped in this bitfield
		// return true if they are, false if not
		bool all_flipped();
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
	struct Piece
	{
		static long long piece_size;
		static long long block_size;
		static uint32_t num_blocks;
		
		std::unique_ptr<BitField> block_bitfield; // a bitfield tracking which blocks have been downloaded by the torrent
		std::unique_ptr<uint8_t[]> data;		  // pointer to a buffer that we write to when we download. Size piece_length.

		std::string piece_hash; // 20 byte SHA1 hash for this piece
		uint32_t piece_index;	// the piece index of this piece

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

		std::vector<Piece> piece_vec;  // vector of pieces, indexed by piece indices
	public:
		std::unique_ptr<BitField> piece_bitfield;	// bitfield of pieces. Used for fast intersection with peer bitfields
		std::queue<Block> block_queue; // queue of block requests that we are currently trying to make

		SingleFileTorrent(std::string metainfo_buffer);

		// Get all unfinished blocks from all unfinished piece vectors,
		// and place these into the block queue
		void update_block_queue();
	};

	// TODO: finish this
	class MultipleFileTorrent : public Torrent
	{
	private:
		std::string name; // the name of the directory to save files
	};
}

#endif