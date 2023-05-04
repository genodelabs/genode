SRC_C      = main.cc
C_WARNINGS = -Wall -Werror
BUILD_DIR  = /tmp/bin
SHELL      = bash

all:
	mkdir -p $(BUILD_DIR)
	g++ $(C_WARNINGS) $(SRC_C) -o $(BUILD_DIR)/file_writer

clean:
	rm -Rf bin
