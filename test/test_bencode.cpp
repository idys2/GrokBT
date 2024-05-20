#include <iostream>
#include <cstring>
#include <assert.h>
#include <fstream>

#include "protocol_utils.h"

int main() {

	// get the ip address of the tracker
	// std::string metainfo = ProtocolUtils::read_metainfo("debian1.torrent");
	std::string metainfo = ProtocolUtils::read_metainfo("bigfile.torrent");
	std::string url = ProtocolUtils::read_announce_url(metainfo);	
	std::cout << "announce url: " << url << std::endl;

	// read info dictionary
	ProtocolUtils::SingleFileInfo ret = ProtocolUtils::read_info_dict(metainfo); 
	
	// is SingleFileInfo correct?
	// in torrent file
	std::cout << std::endl;
	std::cout << "File testing:" << std::endl;
	std::cout << "piece length: " << ret.piece_length << std::endl; 
	std::cout << "pieces string length: " << ret.pieces.length() << std::endl; 
	// length of the pieces string / 20 (20 bytes of the hash are for each piece index)
	std::cout << "number of pieces: " << ret.piece_vec.size() << std::endl; 
	assert(ret.piece_vec.size() == (ret.pieces.length() / 20));
	std::cout << "name: " << ret.name << std::endl; 
	std::cout << "file length: " << ret.length << std::endl;
	std::cout << "block length: " << ret.block_length << std::endl;
	std::cout << std::endl;
	std::cout << "Piece testing:" << std::endl;
	// the first piece
	std::cout << "piece byte length for piece 0: " << ret.piece_vec[0].piece_byte_length << std::endl;
	std::cout << "piece index for piece 0: " << ret.piece_vec[0].piece_index << std::endl;
	std::cout << "piece hash length for piece 0: " << ret.piece_vec[0].hash.length() << std::endl;
	std::cout << "bytes down: " << ret.piece_vec[0].bytes_down << std::endl;
	std::cout << "block size: " << ret.piece_vec[0].block_size << std::endl;
	std::cout << "num blocks: " << ret.piece_vec[0].num_blocks << std::endl;
	std::cout << "output buffer size: " << ret.piece_vec[0].output_buffer.size() << std::endl;

	// get the first piece of the file bigfile.txt (bytes 0, 32767)
	std::ifstream myfile;
	myfile.open("bigfile.txt");

	char *buffer = new char[ret.piece_length];
	myfile.read(buffer, ret.piece_length);

	//std::cout << myfile.gcount() << std::endl;
	myfile.close();

	// the first block btifield
	std::cout << std::endl;
	std::cout << "Bitfield testing: " << std::endl;
	std::cout << "block bitfield vec size: " << ret.piece_vec[0].block_bitfield.bitfield.size() << std::endl;
	std::cout << "num bits: " << ret.piece_vec[0].block_bitfield.num_bits << std::endl;

	std::cout << "first unflipped piece: " << ret.piece_bitfield.first_unflipped() << std::endl;
	std::cout << "all flipped: " << ret.piece_bitfield.all_flipped() << std::endl;
	std::cout << "is first piece set: " << ret.piece_bitfield.is_bit_set(0) << std::endl;
	std::cout << "is second piece set: " << ret.piece_bitfield.is_bit_set(1) << std::endl;

	// do we need a vector of pieces and a piece bitfield?
	std::cout << "first piece first block is done " << ret.piece_vec[0].is_done() << std::endl;

	auto next_req = ret.piece_vec[0].get_next_request();
	std::cout << "next req " << std::get<0>(next_req) << std::endl;
	std::cout << "next req " << std::get<1>(next_req) << std::endl;

	//assert(ret.write(0, std::get<0>(next_req), buffer, std::get<1>(next_req)) == false);
	//ret.piece_vec[0].download_block(std::get<0>(next_req), std::get<1>(next_req), buffer);

	//ret.piece_vec[0].block_bitfield.set_bit(0);

	next_req = ret.piece_vec[0].get_next_request();
	std::cout << "next req " << std::get<0>(next_req) << std::endl;
	std::cout << "next req " << std::get<1>(next_req) << std::endl;

	//assert(ret.write(0, std::get<0>(next_req), buffer, std::get<1>(next_req)));
	//ret.piece_vec[0].download_block(std::get<0>(next_req), std::get<1>(next_req), buffer);

	std::cout << ret.piece_vec[0].verify_hash() << std::endl;

	//ret.piece_vec[0].block_bitfield.set_bit(1);

	std::cout << "first piece is done " << ret.piece_vec[0].is_done() << std::endl;

	/*
	when we write to the buffer directly, the hash is correct
	for(int i = 0; i < ret.piece_length; i++) {
		ret.piece_vec[0].output_buffer[i] = buffer[i];
		assert(ret.piece_vec[0].output_buffer[i] == buffer[i]);
	}
	*/
	
	// works as well
	//ret.piece_vec[0].download_block(0, 16384, buffer);
	//ret.piece_vec[0].download_block(16384, 16384, buffer);
	
	std::cout << "first unflipped piece: " << std::endl;
	std::cout << "first piece first block is done " << ret.piece_vec[ret.piece_bitfield.first_unflipped()].is_done() << std::endl;


	std::string info_dict_as_string = ProtocolUtils::read_info_dict_str(metainfo);
	// std::cout << "bencoded info dict: " << info_dict_as_string << std::endll;
	delete[] buffer;
	myfile.close();

	return 0; 
}
