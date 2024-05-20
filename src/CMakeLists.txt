set(BINARY ${CMAKE_PROJECT_NAME})

file(GLOB_RECURSE SOURCES *.cpp)

# make sure libraries exist
find_library(BENCODE_LIB bencode ${PROJECT_SOURCE_DIR}/lib)
find_library(CURL_LIB curl ${PROJECT_SOURCE_DIR}/lib)

set(SOURCES ${SOURCES})
add_executable(${BINARY} ${SOURCES})

include_directories(${PROJECT_SOURCE_DIR}/include/)

target_link_libraries(${BINARY} ${PROJECT_SOURCE_DIR}/lib/libcurl.so)
target_link_libraries(${BINARY} ${PROJECT_SOURCE_DIR}/lib/libbencode.so)
target_link_libraries(${BINARY} -lcrypto "${BENCODE_LIB}")
add_library(${BINARY}_lib STATIC ${SOURCES})

target_link_libraries(${BINARY}_lib ${PROJECT_SOURCE_DIR}/lib/libcurl.so)
target_link_libraries(${BINARY}_lib ${PROJECT_SOURCE_DIR}/lib/libbencode.so)
target_link_libraries(${BINARY}_lib curl -lcrypto "${BENCODE_LIB}")
