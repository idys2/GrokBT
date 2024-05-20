#ifndef _PROTOCOL_UTILS_H
#define _PROTOCOL_UTILS_H

#include <assert.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <fstream>
#include <cstring>
#include <string>                                                                                     
#include <vector>
#include <bitset>
#include <cmath>

#include "hash.h"
#include "bencode.hpp"

// Utility data structures/functions that are shared between both the 
// Tracker HTTP Protocol and the Peer Wire Protocol to be used by clients
namespace ProtocolUtils { 

	const std::string TORRENT_FOLDER_PATH = "//home//student//fp//torrents//";
	const std::string OUTFILE = "out";
	const long long BLOCK_SIZE =  1 << 14;	// 2^14 = 16KB size of blocks
	sha1sum_ctx *ctx = sha1sum_create(NULL, 0);

	class BitField { 

		public: 
			std::vector<uint8_t> bitfield;
			int num_bits;					// number of bits that this object is representing

			BitField() {}

			// initialize a bitfield with all zeros
			BitField(int b) {
				num_bits = b;
				int len = std::ceil((double) num_bits / 8.0);	 // we can have extra trailing bits at the end
				bitfield = std::vector<uint8_t>(len, 0);
			}

			// given a char pointer to bitfield, copy over bitfield
			BitField(uint8_t* data, int b) {
				num_bits = b;
				int len = std::ceil((double) num_bits / 8.0);	 // we can have extra trailing bits at the end				
				bitfield = std::vector<uint8_t>(len, 0);

				for(int i = 0; i < len; i++) {
					bitfield.at(i) = data[i];	// dangerous?
				}
			}

			// check if bit_index is flipped
			bool is_bit_set(int bit_index) {
				int byte_index = bit_index / 8;	// the index in the vector for this piece index
				int offset = bit_index % 8;		// the bit we are interested in
				return (bitfield.at(byte_index) >> (7 - offset)) & 1 != 0; 
			}

			// sets bit at bit_index to 1
			void set_bit(int bit_index) {

				int byte_index = bit_index / 8;	// the index in the vector for this piece index
				int offset = bit_index % 8;       // the bit we are interested in

				// bitshift the correct bit
				bitfield.at(byte_index) |= 1 << (7 - offset);
			}
			
			// for piece bitfields only!
			// return the first index match with another bitfield
			// a match occurs if we dont have a piece, but the other bitfield does have a piece
			// returns -1 if there are no matches
			int first_match(BitField other) { 
				for(int i = 0; i < num_bits; i++) {
					if(!is_bit_set(i) && other.is_bit_set(i)) {
						return i;
					}
				}
				return -1; 
			}

			/**
			* find first unflipped bit
			* this lets us find the next unflipped piece in the bitfield
			* @returns the bit index of the first unflipped bit, or -1 if all bits are flipped
			*/
			int first_unflipped() { 
				for(int i = 0; i < num_bits; i++) {
					if(!is_bit_set(i)) { 
						return i;
					}
				}
				return -1;
			}

			/**
			 * find first n unflipped bits
			 * NOTE: can (and will) return a vector size <= n, if there are less unflipped bits remaining than queried
			 * @param[in]	n 	number of unflipped bits
			 * @returns a vector of bit indexes for the first n unflipped bits
			*/
			/*std::vector<int> first_n_unflipped(int n) {
				auto unflipped_vec = std::vector<int>();
				for (int i = 0; i < num_bits; i++) {
					if (!is_bit_set(i)) {
						unflipped_vec.push_back(i);
					}
				}
				return unflipped_vec;
			}*/

			// See if all bits in this bitfield have been flipped
			bool all_flipped() {
				for(int i = 0; i < num_bits; i++) {
					if (!is_bit_set(i)) {
						return false;
					}
				}
				return true;
			}
	};

	// Keep track of each piece, and how much we've downloaded for that piece
	class FilePiece { 

		public:

			long long piece_byte_length; 	// the number of bytes for this piece, includes the case of trailing piece size
			uint32_t piece_index; 			// the index of this piece
			std::string hash;				// the 20 byte SHA1 hash for this piece
			long long bytes_down; 			// the number of bytes that we've downloaded for this piece
			uint32_t block_size;		 	// the maximum number of bytes we can request in a single block
			int num_blocks;					// total number of blocks = ceil(piece_byte_length / block_size)
			std::vector<char> output_buffer;		// buffer of blocks/bytes received
			
			// init empty block bitfield
			BitField block_bitfield; 		// a bitfield for indicating which blocks have been downloaded

			FilePiece(long long pl, uint32_t pi, std::string h, uint32_t block_s) {
				piece_byte_length = pl; 
				piece_index = pi; 
				hash = h;
				bytes_down = 0;
				block_size = block_s;

				num_blocks = std::ceil(piece_byte_length / block_size);
				block_bitfield = BitField(num_blocks);
				output_buffer = std::vector<char>(piece_byte_length, 0);
			}

			// download a block by flipping the bit representing that block, 
			// then write the block to the output buffer

			void download_block(int begin, int block_len, std::vector<uint8_t> block) {

				int bit_index = get_block_index(begin, block_len);
				block_bitfield.set_bit(bit_index);

				for(int i = begin; i < begin + block_len; i++) {
					bytes_down++;
					output_buffer.at(i) = block.at(i - begin);	// changed from [] operator
				}
			}

			// check if this piece is done downloading
			bool is_done() {
				return block_bitfield.all_flipped();
			}

			bool verify_hash() { 
				char *piece_hash = output_buffer.data();

				char checksum[20];
				int error = sha1sum_finish(ctx, (const uint8_t *) piece_hash, piece_byte_length, (uint8_t *) checksum);
				assert(!error);
				assert(sha1sum_reset(ctx) == 0);
				std::string p_hash(checksum, 20);

				//std::cout << "Hashed data: " << p_hash << std::endl;
				//std::cout << "Correct hash: " << hash << std::endl;

				return std::string(checksum, 20) == hash;
			}

			// calculate the byte index of this piece in the output file, 
			// then write the entire output buffer to that file
			void write_to_disk(std::ofstream stream) {
				int start_byte_in_file = piece_index * piece_byte_length;	
				stream.seekp(start_byte_in_file, std::ios::beg);
				stream.write(reinterpret_cast<char *>(output_buffer.data()), output_buffer.size());
				
				std::cout << "Wrote " << piece_byte_length << " bytes starting at byte " << start_byte_in_file << std::endl;
			}

			/**
			 * WARNING: ONLY FOR BLOCK BITFIELDS.
			 * returns a pair of data to construct a request message
			 * @returns 0-based byte offset within the piece (block index), 
			 * @returns length of block in bytes (every block length is of equal length 
			 * 			except for the last block, which may be irregular)
			*/
			std::pair<uint32_t, uint32_t> get_next_request() {
				int next_block_idx = block_bitfield.first_unflipped();

				// the last block might not fill a whole block_size worth of bytes
				if(next_block_idx == num_blocks - 1) {
					std::cout << "last block" << std::endl; 
					int remainder = piece_byte_length - next_block_idx * block_size;
					std::cout << "remainder: " << remainder << std::endl; 
					return std::make_pair(next_block_idx * block_size, remainder);
				}
				
				return std::make_pair(next_block_idx * block_size, block_size);
			}

			/**
			 * WARNING: ONLY FOR BLOCK BITFIELDS
			 * returns a vector of pairs of data to construct n request message, where
			 * the pairs of data correspond to the first n unflipped bits.
			 * NOTE: can return a vector size <= n, ALWAYS CHECK USING VECTOR SIZE, NOT n
			 * @param[in]	n 	number of messages to get
			*/
			/*std::vector<std::pair<uint32_t, uint32_t>> get_next_n_requests(int n) {
				auto unflipped_list = block_bitfield.first_n_unflipped(n);
				auto pairs = std::vector<std::pair<uint32_t, uint32_t>>();

				for (auto bit_index : unflipped_list) {

					// the last block might not fill a whole block_size worth of bytes
					if (bit_index == num_blocks - 1) {
						std::cout << "last block" << std::endl; 
						int remainder = piece_byte_length - bit_index * block_size;
						std::cout << "remainder: " << remainder << std::endl; 
						pairs.push_back(std::make_pair(bit_index * block_size, remainder));
					} else {
						pairs.push_back(std::make_pair(bit_index * block_size, block_size));
					}
				}

				return pairs;
			}*/

			// Given a byte off set within this piece, find the block index responsible 
			// for the block [byteoffset, byteoffset + block_size)
			// ASSUMES THAT WE GOT EXACTLY BLOCK_SIZE FOR PIECE
			int get_block_index(int byte_offset, int len) {
				assert(len == block_size);
				return byte_offset / block_size;	
			}

			// prints hex version of string
			void to_hex(std::string s) {
				for (const auto &c : s) {
					std::cout << std::hex << int(c);
				}
				std::cout << std::endl;
			}


	}; 
	
	class SingleFileInfo {

		public:
			// every piece is of equal length except for the final piece, which is irregular
			long long piece_length; // max size of each piece in bytes

			std::string pieces; 	// pieces is a concatenation of 20 byte SHA1 hashes
			std::string name; 		// name of the file
			long long length; 		// total file size
			int block_length;		// max size of each block within a piece

			std::vector<FilePiece> piece_vec;  // ordered list of pieces, indexed by piece index
			// init default bitfield
			BitField piece_bitfield;	// store the bitfield
			
			std::ofstream out_file = std::ofstream(OUTFILE);	// hardcode filepath

			SingleFileInfo () {}
			
			SingleFileInfo(long long pl, std::string p, std::string n, long long len, int bl) {

				piece_length = pl;
				pieces = p; 
				name = n; 
				length = len;

				// set the block length to be the minimum of the piece length, 
				// and the assumed harcoded block length of 2^15
				block_length = std::min(piece_length, BLOCK_SIZE);

				int num_pieces = pieces.length() / 20;	// number of pieces, note: this is always an int
				piece_bitfield = BitField(num_pieces);

				// get sections of the pieces string corresponding to each 20 byte SHA1 hash block
				// create FilePiece objects and populate piece_vec
				for(int curr_index = 0; curr_index < num_pieces - 1; curr_index++) {
					
					std::string hash = pieces.substr(curr_index * 20, 20);
					//FilePiece piece = FilePiece(piece_length, curr_index, hash, block_length);
					piece_vec.push_back(FilePiece(piece_length, curr_index, hash, block_length));
				}

				// the last piece might overshoot in byte size, so find 
				// the number of bytes associated with that piece
				int last_idx = num_pieces - 1;
				int remainder = length - (last_idx) * piece_length;
				std::cout << "last idx: " << last_idx << " " << "remainder " << remainder << std::endl;
				std::string last_hash = pieces.substr(last_idx * 20, 20);
				long long rem_block_length = std::min(remainder, block_length);
				//FilePiece piece = FilePiece(remainder, last_idx, last_hash, block_length);
				piece_vec.push_back(FilePiece(remainder, last_idx, last_hash, rem_block_length));
			}

			/*
			bool write(int piece_index, int begin, char *block, int block_length) {
				//FilePiece piece = piece_vec[piece_index];
				piece_vec[piece_index].download_block(begin, block_length, block);

				// write piece to disk
				// check if the hash matches -> if it does, we are done with this piece
				// write to output file

				if (piece_vec[piece_index].verify_hash()) {

					// flip the bit associated with this piece
					piece_bitfield.set_bit(piece_index);
					piece_vec[piece_index].write_to_disk(std::move(out_file)); 
					return true;
				}

				else {
					std::cout << "verify hash failed!" << std::endl;
				}
				return false;
			}
			*/
	};

	struct Peer {
		// leecher options
        bool am_interested = false;    	// client is interested in the peer
        bool peer_choking = true;      	// peer is choking this client
        
        // seeder options
        bool am_choking = true;        	// client is choking the peer
        bool peer_interested = false;	// peer is interested in this client

        int fd = -1;					// File descriptor for TCP peer connection
		time_t timestamp;				// Timestamp of last message sent
		std::string peer_id = "";
		sockaddr_in sockaddr; 
		BitField piece_bitfield;		// bitfield of which pieces this peer has

		bool shook = false;				// did we handshake with the peer?

		// dictionary mode
		// assumes non network byte order for port! (bittorrent protocol does not specify)
		Peer(std::string pid, std::string ip_addr, int p) {
			inet_pton(AF_INET, ip_addr.c_str(), &(sockaddr.sin_addr));
			sockaddr.sin_port = htons(p); 
			timestamp = time(nullptr);
		}
			
		// binary mode, assumes that the args are passed
		// in network byte order!
		Peer(uint32_t ip_addr, uint16_t p) {
			sockaddr.sin_family = AF_INET;
			sockaddr.sin_port = p; 
			sockaddr.sin_addr.s_addr = ip_addr; // in host order!
			timestamp = time(nullptr);
		}

		// Sets the peer as offline
        void is_inactive() {
            close(fd);
            fd = -1;
        }

		/**
		 * fill in peer's bitfield
		 * @param[in]	bitfield 	pointer to the bitfield
		 * @param[in]	bitfield_length size of bitfield in BYTES
		 */
		void fill_bitfield(uint8_t *bitfield, uint32_t bitfield_length) {
			piece_bitfield = BitField(bitfield, bitfield_length * 8);
		}

		// get std::string representation of sockaddr
		std::string str() {
			char str[INET_ADDRSTRLEN]; 
			inet_ntop(AF_INET, &(sockaddr.sin_addr), str, INET_ADDRSTRLEN); 

			std::string output = "peer ip addr: "+ std::string(str) + " port: " + std::to_string(ntohs(sockaddr.sin_port)); 
			return output;
		}
	};

	/**
	 * From Beej's guide: sendall bytes in buffer via TCP
	 * @param[in] s 	File descriptor
	 * @param[in] buf 	Pointer to buffer to send
	 * @param[in] len 	Pointer to size of buffer
	 * @returns	-1 on failure, 0 on success
	 */
	int sendall(int s, const char *buf, uint32_t *len) {
		int total = 0;        // how many bytes we've sent
		int bytesleft = *len; // how many we have left to send
		int n;

		while(total < *len) {
			n = send(s, buf+total, bytesleft, 0);
			if (n == -1) { break; }
			total += n;
			bytesleft -= n;
		}

		*len = total; // return number actually sent here

		return n==-1?-1:0; // return -1 on failure, 0 on success
	}

	// Compute the 20 byte SHA1 Hash of the bencoded info dictionary string
	// obtained from the metainfo file
	std::string hash_info_dict_str(std::string info_dict_str) {
		char checksum[20]; 
		int error = sha1sum_finish(ctx, (const uint8_t *) info_dict_str.c_str(), info_dict_str.length(), (uint8_t *) checksum);
		assert(!error);
		assert(sha1sum_reset(ctx) == 0);

		return std::string(checksum, 20);
	}
	
	// Compute the 20 byte peer id, using the Azureus style
	// the user provides the client_id, consisting of 
	// 2 characters, following by 4 integers for version number
	// the function produces a unique hash and appends to client id
	std::string unique_peer_id(std::string client_id) {
		assert(client_id.length() == 6);
		std::string peer_id = "-" + client_id + "-";
		std::string pid_str = std::to_string(getpid());  // process id as string 
		std::string timestamp_str = std::to_string(time(nullptr));  // timestamp as string
		std::string input = pid_str + timestamp_str;

		char checksum[20]; 
		int error = sha1sum_finish(ctx, (const uint8_t *) input.c_str(), input.length(), (uint8_t *) checksum);
		assert(!error); 
		assert(sha1sum_reset(ctx) == 0);

		std::string unique_hash = std::string(checksum, 12); // 6 bytes from client_id, + 2 for guards. We take 12 bytes from the hash
		peer_id += unique_hash; 
		assert(peer_id.length() == 20);

		return peer_id;
	}

	// Parse metainfo file into byte buffer
	// the caller must free the buffer returned
	std::string read_metainfo(std::string filename) {
		std::string filepath = TORRENT_FOLDER_PATH + filename;
		std::ifstream stream(filepath, std::ifstream::in);
		
		if(stream) {
			stream.seekg(0, stream.end);
			int length = stream.tellg();
			stream.seekg(0, stream.beg);

			char* buffer = new char[length];
			std::string ret;

			if(stream.read(buffer, length)) {
				ret.assign(buffer, length); 
				delete[] buffer; 
			};

			return ret;
		}

		else {
			std::cout << "file not found" << std::endl;
		}
		
		return "";
	}

	// Read announce url from metainfo file
	// the metainfo_buffer contains a std::string packed with the bencoded data
	std::string read_announce_url(std::string metainfo_buffer) {
		bencode::data data = bencode::decode(metainfo_buffer); 
		auto metainfo_dict = std::get<bencode::dict>(data); 
		return std::get<std::string>(metainfo_dict["announce"]);
	}

	// Read the info dictionary from metainfo file 
	// the metainfo_buffer contains a std::string packed with the bencoded data
	SingleFileInfo read_info_dict(std::string metainfo_buffer) {	
		
		// read from metainfo file
		bencode::data data = bencode::decode(metainfo_buffer);
		auto metainfo_dict = std::get<bencode::dict>(data);
		auto info_dict = std::get<bencode::dict>(metainfo_dict["info"]);

		long long piece_length = std::get<long long>(info_dict["piece length"]);
		std::string pieces = std::get<std::string>(info_dict["pieces"]);
		std::string name = std::get<std::string>(info_dict["name"]);
		long long length = std::get<long long>(info_dict["length"]);

		// set struct fields
		// we use the constant block size that is defined as const in this file
		SingleFileInfo ret = SingleFileInfo{piece_length, pieces, name, length, BLOCK_SIZE};
		return ret;
	}

	// gets std::string representation of the value of info key (returns bencoded dictionary)
	std::string read_info_dict_str(std::string metainfo_buffer) { 
		// read from metainfo file
		bencode::data data = bencode::decode(metainfo_buffer);
		auto metainfo_dict = std::get<bencode::dict>(data);
		auto info_dict = std::get<bencode::dict>(metainfo_dict["info"]);

		// get std::string representation of info dict which is type bencode::dict
		std::string info_dict_str = bencode::encode(info_dict);
		return info_dict_str;
	}
}

#endif
