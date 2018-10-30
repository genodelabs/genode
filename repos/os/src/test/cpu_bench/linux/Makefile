INC_DIR = $(PWD)/..

cpu_bench: main.cc $(INC_DIR)/bogomips.h
	g++ -I$(INC_DIR) -O2 -Wall -Wextra -Weffc++ -std=gnu++11 $< -o $@

clean:
	rm -f *~ cpu_bench
