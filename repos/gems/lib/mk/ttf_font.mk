STB_PORT_DIR := $(call select_from_ports,stb)

SRC_CC  := ttf_font.cc
INC_DIR += $(STB_PORT_DIR)/include
LIBS    += libc libm

# disable warnings caused by stb library
CC_OPT  += -Wno-unused-parameter -Wno-unused-function -Wno-unused-value \
           -Wno-unused-variable

vpath %.cc $(REP_DIR)/src/lib/ttf_font
