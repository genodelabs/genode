SRC_CC = init.cc

vpath %.cc $(REP_DIR)/src/lib/libc_lwip_loopback

LIBS += lwip libc libc_lwip
