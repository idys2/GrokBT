SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin
INC_DIR = include
LIB_DIR = lib

CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17
LDFLAGS = -L$(LIB_DIR)
LDLIBS = -lcrypto -lcurl

SOURCES = $(wildcard $(SRC_DIR)/*.cpp)
OBJECTS = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SOURCES))
EXECUTABLE = $(BIN_DIR)/client
INCLUDE_COMMON = -I$(INC_DIR)

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS) | $(BIN_DIR)
	$(CXX) -o $@ $^ $(LDFLAGS) $(LDLIBS) -Wl,-rpath,$(LIB_DIR)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDE_COMMON) -c -o $@ $<

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

.PHONY: all client server clean
