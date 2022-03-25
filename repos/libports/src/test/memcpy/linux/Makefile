INC_DIR = $(PWD)/..

ifneq (,$(findstring arm-,$(shell gcc -dumpmachine)))
	CC_MARCH = -marm
else
	CC_MARCH =
endif

memcpy: main.cc $(INC_DIR)/memcpy.h
	g++ $(CC_MARCH) -I$(INC_DIR) -O2 -Wall -Wextra -Weffc++ -std=gnu++11 $< -o $@

clean:
	rm -f *~ memcpy
