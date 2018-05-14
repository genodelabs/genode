SRC_CC = init.cc

vpath %.cc $(REP_DIR)/src/lib/libc_lwip_loopback

LIBS += lwip_legacy libc libc_lwip

CC_CXX_WARN_STRICT =
