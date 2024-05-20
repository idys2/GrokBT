cmake_minimum_required(VERSION 3.14)
project(BitTorrent)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include_directories(${PROJECT_SOURCE_DIR}/include)
#include_directories(${PROJECT_SOURCE_DIR}/hash)

link_directories(${PROJECT_SOURCE_DIR}/lib)

#add_subdirectory(hash)
add_subdirectory(test)
#add_subdirectory(src)
