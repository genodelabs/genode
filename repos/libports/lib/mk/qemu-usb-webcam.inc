include $(REP_DIR)/lib/mk/qemu-usb.inc

LIBS += libc
LIBS += libyuv
LIBS += format

SRC_CC += webcam.cc
SRC_C  += dev-webcam.c

vpath dev-webcam.c $(LIB_DIR)

SHARED_LIB = yes
