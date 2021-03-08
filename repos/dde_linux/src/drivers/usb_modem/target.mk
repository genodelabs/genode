TARGET  := usb_modem_drv
SRC_C   := dummies.c lxc.c
SRC_CC  := main.cc lx_emul.cc component.cc terminal.cc fec_nic.cc
SRC_CC  += printf.cc bug.cc timer.cc scheduler.cc malloc.cc env.cc work.cc
SRC_CC  += uplink_client.cc

LIBS    := base usb_modem_include lx_kit_setjmp nic_driver

USB_CONTRIB_DIR := $(call select_from_ports,dde_linux)/src/drivers/usb_modem

INC_DIR += $(PRG_DIR)
INC_DIR += $(REP_DIR)/src/include

SRC_C += drivers/net/usb/usbnet.c
SRC_C += drivers/net/usb/cdc_ncm.c
SRC_C += drivers/net/usb/cdc_mbim.c
SRC_C += drivers/usb/class/cdc-wdm.c
SRC_C += net/core/skbuff.c
SRC_C += net/ethernet/eth.c

CC_C_OPT += -Wno-comment -Wno-int-conversion -Wno-incompatible-pointer-types \
            -Wno-unused-variable -Wno-pointer-sign -Wno-uninitialized \
            -Wno-maybe-uninitialized -Wno-format -Wno-discarded-qualifiers \
            -Wno-unused-function -Wno-unused-but-set-variable

CC_CXX_WARN_STRICT =

vpath %.c  $(USB_CONTRIB_DIR)
vpath %.cc $(REP_DIR)/src/lx_kit
