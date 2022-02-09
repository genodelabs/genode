TARGET = test-smartcard
LIBS   = base pcsc-lite posix libusb ccid
SRC_CC = main.cc

vpath main.cc $(PRG_DIR)/..

CC_CXX_WARN_STRICT =
