TARGET  = usb_net
LIBS    = base jitterentropy virt_lx_emul

INC_DIR = $(PRG_DIR)/../..

SRC_C += dummies.c \
         generated_dummies.c \
         lx_emul.c \
         lx_user.c

SRC_C += lx_emul/virt/shadow/drivers/usb/core/buffer.c
SRC_C += lx_emul/virt/shadow/drivers/usb/core/hcd.c
SRC_C += lx_emul/virt/usb_client.c

SRC_CC  = main.cc \
          wdm_terminal.cc

SRC_CC += lx_emul/random.cc

SRC_CC += genode_c_api/mac_address_reporter.cc \
          genode_c_api/usb_client.cc \
          genode_c_api/uplink.cc


vpath %.c  $(PRG_DIR)
vpath %.c  $(PRG_DIR)/../..
vpath %.cc $(PRG_DIR)/../..
vpath %.c  $(REP_DIR)/src/lib

C_API = $(dir $(call select_from_repositories,src/lib/genode_c_api))

vpath genode_c_api/mac_address_reporter.cc $(C_API)
vpath genode_c_api/uplink.cc               $(C_API)
vpath genode_c_api/usb_client.cc           $(C_API)

# vim: set ft=make :
