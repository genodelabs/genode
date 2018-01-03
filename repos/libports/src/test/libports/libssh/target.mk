TARGET = test-libssh
LIBS   = base libcrypto libssh
SRC_CC = main.cc

vpath main.cc $(PRG_DIR)/..

CC_CXX_WARN_STRICT =
