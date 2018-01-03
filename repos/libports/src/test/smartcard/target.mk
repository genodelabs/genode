TARGET = test-smartcard
LIBS   = pcsc-lite posix
SRC_CC = main.cc

vpath main.cc $(PRG_DIR)/..

CC_CXX_WARN_STRICT =
