SRC_CC = init.cc plugin.cc

vpath %.cc $(REP_DIR)/src/lib/libc_lwip

LIBS += lwip libc libc-resolv libc-isc libc-nameser libc-net libc-rpc
