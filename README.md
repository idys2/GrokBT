# BitTorrent
This is a C++ implementation of a CLI bit torrent. 

## File Structure
- ```include``` folder holds all header files (.h)
- ```lib``` folder holds all external libraries
- ```src``` folder holds all source code (.cpp)
- ```test``` folder for unit tests

## Dependencies
- libcurl
- bencode.hpp for bencode/bdecode
- argparse.hpp for cli argument parse
- openssl

## Usage 
```
make 
./bin/client
```

## TODO 
- Get tracker HTTP request working again 
- Figure out non blocking socket logic