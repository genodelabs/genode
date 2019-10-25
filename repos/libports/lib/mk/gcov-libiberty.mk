GCOV_PORT_DIR := $(call select_from_ports,gcov)

GCOV_DIR := $(GCOV_PORT_DIR)/src/gcov

SRC_C = argv.c \
        concat.c \
        cp-demangle.c \
        cplus-dem.c \
        d-demangle.c \
        filename_cmp.c \
        fopen_unlocked.c \
        hashtab.c \
        lbasename.c \
        md5.c \
        obstack.c \
        rust-demangle.c \
        safe-ctype.c \
        splay-tree.c \
        vprintf-support.c \
        xexit.c \
        xmalloc.c \
        xmemdup.c \
        xstrdup.c \
        xstrerror.c \
        xstrndup.c \
        xvasprintf.c

CC_OPT += -DHAVE_CONFIG_H

LIBS += libc stdcxx

INC_DIR += $(GCOV_DIR)/include

ifeq ($(filter-out $(SPECS),arm),)
	INC_DIR += $(GCOV_PORT_DIR)/include/arm/libiberty
endif

ifeq ($(filter-out $(SPECS),arm_64),)
	INC_DIR += $(GCOV_PORT_DIR)/include/arm_64/libiberty
endif

ifeq ($(filter-out $(SPECS),x86_32),)
	INC_DIR += $(GCOV_PORT_DIR)/include/x86_32/libiberty
endif

ifeq ($(filter-out $(SPECS),x86_64),)
	INC_DIR += $(GCOV_PORT_DIR)/include/x86_64/libiberty
endif

vpath %.c $(GCOV_DIR)/libiberty

CC_CXX_WARN_STRICT =
