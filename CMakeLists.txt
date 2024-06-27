cmake_minimum_required(VERSION 3.14)
include(FetchContent)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
project(newBT)

# find libcurl and openssl

find_package(CURL QUIET)
if(NOT CURL_FOUND)
    message("Curl not found, fetching...")
    FetchContent_Declare(
        CURL
        GIT_REPOSITORY https://github.com/curl/curl.git
    )
    FetchContent_MakeAvailable(CURL)

endif()

find_package(OpenSSL QUIET)
if(NOT OPENSSL_FOUND)
    message("OpenSSL not found, fetching...")
    FetchContent_Declare(
        OPENSSL
        GIT_REPOSITORY https://github.com/openssl/openssl.git
    )
    FetchContent_MakeAvailable(OpenSSL)
endif()

# find bencode and argparse

FetchContent_Declare(
  bencode
  GIT_REPOSITORY https://github.com/jimporter/bencode.hpp.git
)

FetchContent_Declare(
  argparse
  GIT_REPOSITORY https://github.com/p-ranav/argparse.git
)

FetchContent_MakeAvailable(argparse bencode)

add_library(TorrentModule 
            src/client.cpp 
            src/hash.cpp 
            src/tracker_protocol.cpp
            src/tcp_utils.cpp
            src/peer.cpp
            src/metainfo.cpp
            src/message.cpp
            src/file.cpp
            )

# set target libcurl and openssl
if(NOT TARGET CURL::libcurl)
    add_library(CURL::libcurl INTERFACE IMPORTED)
    set_target_properties(CURL::libcurl PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${CURL_INCLUDE_DIRS}"
        INTERFACE_LINK_LIBRARIES "${CURL_LIBRARIES}"
    )
endif()

if(NOT TARGET OpenSSL::SSL)
    add_library(OpenSSL::SSL INTERFACE IMPORTED)
    set_target_properties(OPENSSL::SSL PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIRS}"
        INTERFACE_LINK_LIBRARIES "${OPENSSL_LIBRARIES}"
    )
endif()
target_link_libraries(TorrentModule CURL::libcurl OpenSSL::SSL)

# include all header files
target_include_directories(TorrentModule PRIVATE ${CURL_SOURCE_DIR} ${CURL_SOURCE_DIR}/include)
target_include_directories(TorrentModule PRIVATE ${OPENSSL_SOURCE_DIR} ${OPENSSL_SOURCE_DIR}/include)
target_include_directories(TorrentModule PUBLIC ${argparse_SOURCE_DIR} ${argparse_SOURCE_DIR}/include/)
target_include_directories(TorrentModule PUBLIC ${bencode_SOURCE_DIR} ${bencode_SOURCE_DIR}/include/)
target_include_directories(TorrentModule PUBLIC ${CMAKE_SOURCE_DIR}/include)

add_executable(torrent src/main.cpp)
target_link_libraries(torrent TorrentModule)
