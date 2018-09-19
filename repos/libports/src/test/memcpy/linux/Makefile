INC_DIR = $(PWD)/..

memcpy: main.cc $(INC_DIR)/memcpy.h
	g++ -I$(INC_DIR) -O2 -Wall -Wextra -Weffc++ -std=gnu++11 $< -o $@

clean:
	rm -f *~ memcpy
