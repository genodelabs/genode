INC_DIR = $(PWD)/..

ifneq (,$(findstring arm-,$(shell gcc -dumpmachine)))
	CC_MARCH = -marm
else
	CC_MARCH =
endif

cache: main.cc $(INC_DIR)/common.h
	g++ -I$(INC_DIR) $(CC_MARCH) -O2 -Wall -Wextra -Weffc++ -std=gnu++11 $< -o $@

clean:
	rm -f *~ cache
