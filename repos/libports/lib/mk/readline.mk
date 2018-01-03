READLINE_PORT_DIR := $(call select_from_ports,readline)
READLINE_DIR      := $(READLINE_PORT_DIR)/src/lib/readline

LIBS += libc

# use our customized 'config.h'
INC_DIR += $(REP_DIR)/include/readline
INC_DIR += $(READLINE_PORT_DIR)/include

# add local readline headers to include-search path
INC_DIR += $(READLINE_DIR)/src/base

CC_DEF += -DHAVE_CONFIG_H
CC_DEF += -DRL_LIBRARY_VERSION='"6.0"'

# dim build noise for contrib code
CC_WARN = -Wno-unused-but-set-variable

# sources from readline base directory
SRC_C = \
	readline.c vi_mode.c funmap.c keymaps.c parens.c search.c rltty.c \
	complete.c bind.c isearch.c display.c signals.c util.c kill.c undo.c \
	macro.c input.c callback.c terminal.c text.c nls.c misc.c xmalloc.c \
	history.c histexpand.c histfile.c histsearch.c shell.c mbutil.c tilde.c \
	compat.c

SRC_CC += genode.cc

vpath %.c       $(READLINE_DIR)
vpath genode.cc $(REP_DIR)/src/lib/readline

SHARED_LIB = yes

CC_CXX_WARN_STRICT =
