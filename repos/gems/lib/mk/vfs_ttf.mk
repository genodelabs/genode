SRC_CC = vfs.cc

INC_DIR += $(REP_DIR)/src/lib/vfs/ttf

LIBS  += ttf_font

vpath %.cc $(REP_DIR)/src/lib/vfs/ttf

SHARED_LIB = yes
