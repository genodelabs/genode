include $(REP_DIR)/ports/pcre.inc

PCRE_DIR = $(REP_DIR)/contrib/$(PCRE)

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

INC_DIR += $(PCRE_DIR)
INC_DIR += $(REP_DIR)/src/lib/pcre/include

CC_OPT += -DHAVE_CONFIG_H

LIBS += libc zlib readline

SHARED_LIB = yes

vpath %.c $(PCRE_DIR)
vpath pcre_chartables.c $(REP_DIR)/src/lib/pcre
