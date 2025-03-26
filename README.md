# BitTorrent

A bittorrent client made to...grok bittorrent. And C++! 

- bencode.hpp to bencode/bdecode torrent files 
- libcurl to manipulate url strings 
- openssl for SHA1 hashing
- argparse to handle command line arguments

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
./torrent
```

## TODO 
