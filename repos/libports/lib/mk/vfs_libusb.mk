SRC_CC = vfs_libusb.cc

LIBS += libusb

vpath %.cc $(REP_DIR)/src/lib/vfs/libusb

SHARED_LIB = yes

CC_CXX_WARN_STRICT =
