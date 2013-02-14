TARGET = test-libssl
LIBS   = libc libcrypto libssl
SRC_CC = main.cc

vpath main.cc $(PRG_DIR)/..
