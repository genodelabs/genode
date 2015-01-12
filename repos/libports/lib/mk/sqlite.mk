SQLITE_DIR = $(call select_from_ports,sqlite)/src/lib/sqlite

INC_DIR += $(SQLITE_DIR)

LIBS += libc

SRC_C = sqlite3.c

vpath %.c $(SQLITE_DIR)
