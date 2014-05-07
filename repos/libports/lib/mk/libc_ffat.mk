SRC_CC  = plugin.cc
LIBS   += libc ffat_block

vpath plugin.cc $(REP_DIR)/src/lib/libc_ffat

SHARED_LIB = yes
