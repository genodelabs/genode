include $(REP_DIR)/lib/import/import-pcre.mk

PCRE_PORT_DIR := $(call select_from_ports,pcre)

SRC_C = pcre_byte_order.c   \
        pcre_chartables.c   \
        pcre_compile.c      \
        pcre_config.c       \
        pcre_dfa_exec.c     \
        pcre_exec.c         \
        pcre_fullinfo.c     \
        pcre_get.c          \
        pcre_globals.c      \
        pcre_maketables.c   \
        pcre_newline.c      \
        pcre_ord2utf8.c     \
        pcre_refcount.c     \
        pcre_string_utils.c \
        pcre_study.c        \
        pcre_tables.c       \
        pcre_ucd.c          \
        pcre_valid_utf8.c   \
        pcre_version.c      \
        pcre_xclass.c

INC_DIR += $(PCRE_PORT_DIR)/src/lib/pcre \
           $(REP_DIR)/src/lib/pcre/include

CC_OPT += -DHAVE_CONFIG_H

LIBS += libc

SHARED_LIB = yes

vpath %.c $(PCRE_PORT_DIR)/src/lib/pcre
vpath pcre_chartables.c $(REP_DIR)/src/lib/pcre
