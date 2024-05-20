# BitTorrent2

## File Structure
- ```include``` folder holds all header files (.hpp)
- ```lib``` folder holds all external libraries
- ```src``` folder holds all source code (.cpp)
- ```test``` folder for random testing

## Usage
```
mkdir build 
cd build 
cmake .. 
cmake --build . 
```

## Roles
- Richard: bencoding, tracker
- Jason: leecher
- Ryan: seeder
- Alex: unit tests