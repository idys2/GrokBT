# BitTorrent

This is a C++ implementation of a command line bittorrent protocol. The goal is to write as much from scratch as possible, so we only use the following libraries in the following ways: s

- bencode.hpp to bencode/bdecode torrent files 
- libcurl to manipulate url strings 
- openssl for SHA1 hashing
- argparse to handle command line arguments

All other parts of the BitTorrent protocol are from scratch.

## File Structure
- ```include``` folder holds all header files (.h)
- ```src``` folder holds all source code (.cpp)
- ```test``` folder for unit tests

## Usage 
```
mkdir build 
cd build 
cmake ..  
make
./client
```

## TODO 
- Get tracker HTTP request working again 
- Figure out non blocking socket logic
