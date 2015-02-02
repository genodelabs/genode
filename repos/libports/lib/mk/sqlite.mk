SQLITE_DIR = $(call select_from_ports,sqlite)/src/lib/sqlite

INC_DIR += $(SQLITE_DIR)

LIBS += libc pthread jitterentropy

SRC_C = sqlite3.c
SRC_CC = genode_vfs.cc

# https://github.com/genodelabs/genode/issues/754
CC_OPT += -DSQLITE_4_BYTE_ALIGNED_MALLOC

# Defenestrate the Unix and Windows OS layers and use our own.
CC_OPT += -DSQLITE_OS_OTHER=1
CC_OPT_sqlite3 += -Wno-unused

vpath %.c $(SQLITE_DIR)/
vpath %.cc $(REP_DIR)/src/lib/sqlite
