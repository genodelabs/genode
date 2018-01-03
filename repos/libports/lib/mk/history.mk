READLINE_PORT_DIR := $(call select_from_ports,readline)
READLINE_DIR      := $(READLINE_PORT_DIR)/src/lib/readline

LIBS += libc

# use our customized 'config.h'
INC_DIR += $(REP_DIR)/include/readline
INC_DIR += $(READLINE_PORT_DIR)/include/readline

# add local readline headers to include-search path
INC_DIR += $(READLINE_DIR)

CC_DEF += -DHAVE_CONFIG_H
CC_DEF += -DRL_LIBRARY_VERSION='"6.0"'

# dim build noise for contrib code
CC_WARN = -Wno-unused-but-set-variable

# sources from readline base directory
SRC_C = \
	history.c histexpand.c histfile.c histsearch.c shell.c mbutil.c \
	xmalloc.c

vpath %.c $(READLINE_DIR)

SHARED_LIB = yes

CC_CXX_WARN_STRICT =
