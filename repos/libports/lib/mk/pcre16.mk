include $(call select_from_repositories,lib/import/import-pcre.mk)

PCRE_PORT_DIR := $(call select_from_ports,pcre)

SRC_C = pcre16_byte_order.c   \
        pcre16_chartables.c   \
        pcre16_compile.c      \
        pcre16_config.c       \
        pcre16_dfa_exec.c     \
        pcre16_exec.c         \
        pcre16_fullinfo.c     \
        pcre16_get.c          \
        pcre16_globals.c      \
        pcre16_jit_compile.c  \
        pcre16_maketables.c   \
        pcre16_newline.c      \
        pcre16_ord2utf16.c    \
        pcre16_refcount.c     \
        pcre16_string_utils.c \
        pcre16_study.c        \
        pcre16_tables.c       \
        pcre16_ucd.c          \
        pcre16_utf16_utils.c  \
        pcre16_valid_utf16.c  \
        pcre16_version.c      \
        pcre16_xclass.c

INC_DIR += $(PCRE_PORT_DIR)/src/lib/pcre \
           $(REP_DIR)/src/lib/pcre \
           $(REP_DIR)/src/lib/pcre/include

CC_OPT += -DHAVE_CONFIG_H

LIBS += libc

SHARED_LIB = yes

vpath %.c $(PCRE_PORT_DIR)/src/lib/pcre

CC_CXX_WARN_STRICT =
