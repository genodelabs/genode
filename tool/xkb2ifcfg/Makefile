#
# Check config
#

ifeq ($(filter clean cleanall, $(MAKECMDGOALS)),)
GENODE_DIR := $(realpath $(dir $(MAKEFILE_LIST))/../..)
ifneq ($(shell pkg-config --print-errors --errors-to-stdout --exists xkbcommon),)
$(error Please install libxkbcommon-dev)
endif
endif

#
# Build rules
#

TARGET = xkb2ifcfg

SRC_CC  = $(wildcard *.cc)
SRC_CC += $(GENODE_DIR)/repos/base/src/lib/base/xml_generator.cc
SRC_CC += $(GENODE_DIR)/repos/base/src/lib/base/output.cc
SRC_H   = $(wildcard *.h)

CFLAGS  = -Werror -Wall -Wextra -Wno-attributes -std=gnu++20 -ggdb
CFLAGS += -I$(GENODE_DIR)/repos/os/include
CFLAGS += -I$(GENODE_DIR)/repos/base/src/include
CFLAGS += -I$(GENODE_DIR)/repos/base/include
CFLAGS += -I$(GENODE_DIR)/repos/base/include/spec/64bit
CFLAGS += -I$(GENODE_DIR)/repos/base/include/spec/x86
CFLAGS += -I$(GENODE_DIR)/repos/base/include/spec/x86_64
CFLAGS += $(shell pkg-config --cflags --libs xkbcommon)

$(TARGET): $(SRC_CC) $(SRC_H) Makefile $(OBJECTS)
	g++ -o $@ $(SRC_CC) $(OBJECTS) $(CFLAGS)

cleanall clean:
	rm -f $(TARGET) *~

.PHONY: cleanall clean
