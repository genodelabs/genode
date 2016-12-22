TARGET = test-libc_noux
LIBS   = posix libc_noux
SRC_CC = main.cc

# we re-use the libc_ffat test
vpath main.cc $(call select_from_repositories,src/test/libc_ffat)
