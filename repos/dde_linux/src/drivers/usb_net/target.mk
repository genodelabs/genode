TARGET  := usb_net_drv
SRC_C   := dummies.c lxc.c
SRC_CC  := main.cc lx_emul.cc linux_network_session_base.cc
SRC_CC  += printf.cc bug.cc timer.cc scheduler.cc malloc.cc env.cc work.cc
SRC_CC  += uplink_client.cc

LIBS    := base usb_net_include lx_kit_setjmp nic_driver format

USB_CONTRIB_DIR := $(call select_from_ports,dde_linux)/src/drivers/usb_net

INC_DIR += $(PRG_DIR)
INC_DIR += $(REP_DIR)/src/include
INC_DIR += $(REP_DIR)/src/drivers/nic

SRC_C += drivers/net/usb/asix_common.c
SRC_C += drivers/net/usb/asix_devices.c
SRC_C += drivers/net/usb/ax88172a.c
SRC_C += drivers/net/usb/ax88179_178a.c
SRC_C += drivers/net/usb/cdc_ether.c
SRC_C += drivers/net/usb/rndis_host.c
SRC_C += drivers/net/usb/smsc95xx.c
SRC_C += drivers/net/usb/usbnet.c
SRC_C += fs/nls/nls_base.c
SRC_C += lib/ctype.c
SRC_C += lib/hexdump.c
SRC_C += net/core/skbuff.c
SRC_C += net/ethernet/eth.c

CC_OPT += -Wno-address-of-packed-member

CC_C_OPT += -Wno-comment -Wno-int-conversion -Wno-incompatible-pointer-types \
            -Wno-unused-variable -Wno-pointer-sign -Wno-uninitialized \
            -Wno-maybe-uninitialized -Wno-format -Wno-discarded-qualifiers \
            -Wno-unused-function -Wno-unused-but-set-variable

CC_CXX_WARN_STRICT =

vpath linux_network_session_base.cc $(REP_DIR)/src/drivers/nic
vpath %.c  $(USB_CONTRIB_DIR)
vpath %.cc $(REP_DIR)/src/lib/legacy/lx_kit
