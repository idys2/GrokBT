cmake_minimum_required(VERSION 3.14)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
project(newBT)

set(LIBCURL_ROOT ${CMAKE_SOURCE_DIR}/libcurl)
set(OPENSSL_ROOT ${CMAKE_SOURCE_DIR}/openssl)

if(POLICY CMP0144)
    cmake_policy(SET CMP0144 NEW)
endif()

if(EXISTS ${LIBCURL_ROOT}/install)
    set(CURL_INCLUDE_DIRS ${LIBCURL_ROOT}/install/include)
    set(CURL_LIBRARIES ${LIBCURL_ROOT}/install/lib/libcurl.a)
    if(NOT TARGET CURL::libcurl)
        add_library(CURL::libcurl INTERFACE IMPORTED)
        set_target_properties(CURL::libcurl PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${CURL_INCLUDE_DIRS}"
            INTERFACE_LINK_LIBRARIES "${CURL_LIBRARIES}"
        )
    endif()
else()
    find_package(CURL QUIET)
    if(NOT CURL_FOUND)
        message(STATUS "libcurl not found, installing...")

        ExternalProject_Add(libcurl_project
            GIT_REPOSITORY https://github.com/curl/curl.git
            PREFIX ${LIBCURL_ROOT}
            CONFIGURE_COMMAND "./configure --prefix=${LIBCURL_ROOT}/install"
            BUILD_COMMAND "make"
            INSTALL_COMMAND "make install"
        )

        set(CURL_INCLUDE_DIRS ${LIBCURL_ROOT}/install/include)
        set(CURL_LIBRARIES ${LIBCURL_ROOT}/install/lib/libcurl.a)
        if(NOT TARGET CURL::libcurl)
            add_library(CURL::libcurl INTERFACE IMPORTED)
            set_target_properties(CURL::libcurl PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${CURL_INCLUDE_DIRS}"
                INTERFACE_LINK_LIBRARIES "${CURL_LIBRARIES}"
            )
        endif()
    else()
        message(STATUS "libcurl found.")
        if(NOT TARGET CURL::libcurl)
            add_library(CURL::libcurl INTERFACE IMPORTED)
            set_target_properties(CURL::libcurl PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${CURL_INCLUDE_DIRS}"
                INTERFACE_LINK_LIBRARIES "${CURL_LIBRARIES}"
            )
        endif()
    endif()
endif()

if(EXISTS ${OPENSSL_ROOT}/install)
    set(OPENSSL_INCLUDE_DIRS ${OPENSSL_ROOT}/install/include)
    set(OPENSSL_LIBRARIES ${OPENSSL_ROOT}/install/lib/libssl.a;${OPENSSL_ROOT}/install/lib/libcrypto.a)
    if(NOT TARGET OpenSSL::SSL)
        add_library(OpenSSL::SSL INTERFACE IMPORTED)
        set_target_properties(OpenSSL::SSL PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIRS}"
            INTERFACE_LINK_LIBRARIES "${OPENSSL_LIBRARIES}"
        )
    endif()
else()
    find_package(OpenSSL QUIET)
    if(NOT OPENSSL_FOUND)
        message(STATUS "OpenSSL not found, installing...")
        ExternalProject_Add(openssl_project
            GIT_REPOSITORY https://github.com/openssl/openssl.git
            PREFIX ${OPENSSL_ROOT}
            CONFIGURE_COMMAND "./config --prefix=${OPENSSL_ROOT}/install"
            BUILD_COMMAND "make"
            INSTALL_COMMAND "make install"
        )
        set(OPENSSL_INCLUDE_DIRS ${OPENSSL_ROOT}/install/include)
        set(OPENSSL_LIBRARIES ${OPENSSL_ROOT}/install/lib/libssl.a;${OPENSSL_ROOT}/install/lib/libcrypto.a)
        if(NOT TARGET OpenSSL::SSL)
            add_library(OpenSSL::SSL INTERFACE IMPORTED)
            set_target_properties(OpenSSL::SSL PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIRS}"
                INTERFACE_LINK_LIBRARIES "${OPENSSL_LIBRARIES}"
            )
        endif()
    else()
        message(STATUS "OpenSSL found.")
        if(NOT TARGET OpenSSL::SSL)
            add_library(OpenSSL::SSL INTERFACE IMPORTED)
            set_target_properties(OpenSSL::SSL PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIRS}"
                INTERFACE_LINK_LIBRARIES "${OPENSSL_LIBRARIES}"
            )
        endif()
    endif()
endif()

# find bencode and argparse

include(FetchContent)
FetchContent_Declare(
  bencode
  GIT_REPOSITORY https://github.com/jimporter/bencode.hpp.git
)

FetchContent_Declare(
  argparse
  GIT_REPOSITORY https://github.com/p-ranav/argparse.git
)

FetchContent_MakeAvailable(argparse bencode)

# compile hash as its own library
add_subdirectory(hash)

add_executable(client src/client.cpp)

target_include_directories(client PUBLIC
                          "${PROJECT_BINARY_DIR}"
                          "${PROJECT_SOURCE_DIR}/hash"
                          )

set(INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)
target_include_directories(client PRIVATE ${INCLUDE_DIR})
target_include_directories(client PRIVATE ${argparse_SOURCE_DIR} ${argparse_SOURCE_DIR}/include/)
target_include_directories(client PRIVATE ${bencode_SOURCE_DIR} ${bencode_SOURCE_DIR}/include/)

target_link_libraries(client PRIVATE CURL::libcurl OpenSSL::SSL)
target_link_libraries(client PUBLIC hash)
