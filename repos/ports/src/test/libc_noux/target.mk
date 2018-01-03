TARGET = test-libc_noux
LIBS   = libc libc_noux
SRC_CC = main.cc

# we re-use the libc_vfs test
vpath main.cc $(call select_from_repositories,src/test/libc_vfs)

CC_CXX_WARN_STRICT =
