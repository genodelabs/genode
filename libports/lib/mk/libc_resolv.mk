LIBS   = libc libc-resolv libc-isc libc-nameser libc-net libc-rpc

SRC_CC = plugin.cc

vpath %.cc $(REP_DIR)/src/lib/libc_resolv

include $(REP_DIR)/lib/mk/libc-common.inc

SHARED_LIB = yes
