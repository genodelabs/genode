# we need pcsc-lite headers, but cannot add pcsc-lite to LIBS because of circular dependency
include $(call select_from_repositories,lib/import/import-pcsc-lite.mk)

LIBCCID_DIR := $(call select_from_ports,ccid)/src/lib/ccid
LIBS        += libusb libc

# find 'config.h'
INC_DIR += $(REP_DIR)/src/lib/ccid

INC_DIR += $(LIBCCID_DIR)/src

SRC_C = ccid.c \
        commands.c \
        ifdhandler.c \
        utils.c \
        ccid_usb.c \
        tokenparser.c \
        towitoko/atr.c \
        towitoko/pps.c \
        openct/buffer.c \
        openct/checksum.c \
        openct/proto-t1.c

INFO_PLIST := $(BUILD_BASE_DIR)/bin/Info.plist

HOST_TOOLS += $(INFO_PLIST)

$(INFO_PLIST): $(LIBCCID_DIR)/src/Info.plist.src $(LIBCCID_DIR)/readers/supported_readers.txt
	$(LIBCCID_DIR)/src/create_Info_plist.pl $(LIBCCID_DIR)/readers/supported_readers.txt $(LIBCCID_DIR)/src/Info.plist.src --target=dummy --version=dummy > $@

vpath %.c $(LIBCCID_DIR)/src

SHARED_LIB = yes

CC_CXX_WARN_STRICT =
