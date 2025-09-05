TARGET  = usb_serial
LIBS    = base jitterentropy virt_lx_emul

INC_DIR = $(PRG_DIR)/../.. $(REP_DIR)/src/lib/usb_client

SRC_C += dummies.c
SRC_C += generated_dummies.c
SRC_C += lx_emul.c
SRC_C += lx_user.c
SRC_C += lx_emul/virt/shadow/drivers/usb/core/buffer.c
SRC_C += lx_emul/virt/shadow/drivers/usb/core/hcd.c
SRC_C += lx_emul/virt/usb_client.c

SRC_CC += genode_c_api/terminal.cc
SRC_CC += genode_c_api/usb_client.cc
SRC_CC += lx_emul/random.cc
SRC_CC += main.cc

vpath %.c  $(PRG_DIR)
vpath %.c  $(PRG_DIR)/../..
vpath %.cc $(PRG_DIR)/../..
vpath %.c  $(REP_DIR)/src/lib
vpath %.cc $(REP_DIR)/src/lib

C_API = $(dir $(call select_from_repositories,src/lib/genode_c_api))

vpath genode_c_api/usb_client.cc $(C_API)
vpath genode_c_api/terminal.cc $(C_API)

# vim: set ft=make :
