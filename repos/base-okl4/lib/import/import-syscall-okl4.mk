OKL4_DIR = $(call select_from_ports,okl4)/src/kernel/okl4

#
# Make sure that symlink modification times are handled correctly.
# Otherwise, the creation of symlinks that depend on their own directory
# behaves like a phony rule. This is because the directory mtime is
# determined by taking the mtimes of containing symlinks into account.
# Hence, all symlinks (except for the youngest) depend on a directory
# with a newer mtime. The make flag -L fixes the problem. Alternatively,
# we could use 'cp' instead of 'ln'.
#
MAKEFLAGS += -L

#
# OKL4-specific headers
#
INC_DIR += $(LIB_CACHE_DIR)/syscall-okl4/include

#
# Define maximum number of threads, needed by the OKL4 kernel headers
#
CC_OPT += -DCONFIG_MAX_THREAD_BITS=10

