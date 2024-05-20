file(GLOB_RECURSE TESTS *.cpp)

set(TESTS ${TESTS})

find_library(BENCODE_LIB bencode ${PROJECT_SOURCE_DIR}/lib/)
include_directories(${PROJECT_SOURCE_DIR}/include/)
#include_directories(${PROJECT_SOURCE_DIR}/hash/)

foreach (test_source ${TESTS})
	
	# get file name and replace .cpp with nothing for executable 
	get_filename_component(test_filename ${test_source} NAME)
	string(REPLACE ".cpp" "" test_name ${test_filename})

	add_executable(${test_name} ${test_source})
	target_compile_features(${test_name} PUBLIC cxx_std_17)
	target_link_libraries(${test_name} PUBLIC ${PROJECT_SOURCE_DIR}/lib/libbencode.so ${PROJECT_SOURCE_DIR}/lib/libcurl.so ${PROJECT_SOURCE_DIR}/lib/hash.o -lcrypto)
endforeach(test_source ${TESTS})
