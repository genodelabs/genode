SRC_CC = init.cc plugin.cc

vpath %.cc $(REP_DIR)/src/lib/libc_lxip

LIBS += lxip libc libc_resolv
