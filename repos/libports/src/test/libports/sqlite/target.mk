SQLITE_DIR = $(call select_from_ports,sqlite)/src/lib/sqlite

TARGET = test-sqlite
LIBS   = libc sqlite

# This file contains code to implement the "sqlite" command line
# utility for accessing SQLite databases.
SRC_CC = shell.c

vpath %.c $(SQLITE_DIR)
