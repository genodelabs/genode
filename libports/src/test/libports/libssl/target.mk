TARGET = test-libssl
LIBS   = cxx env libcrypto libssl
SRC_CC = main.cc

vpath main.cc $(PRG_DIR)/..
