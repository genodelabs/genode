TARGET  = usb_hid_drv
LIBS    = base jitterentropy virt_lx_emul

INC_DIR = $(PRG_DIR)/../.. $(REP_DIR)/src/lib/usb_client

SRC_C += dummies.c \
         generated_dummies.c \
         lx_emul.c \
         lx_user.c

SRC_C += lx_emul/shadow/drivers/input/evdev.c \
         lx_emul/shadow/drivers/input/input-leds.c \
         lx_emul/virt/shadow/drivers/usb/core/message.c \
         lx_emul/virt/shadow/drivers/usb/core/urb.c \
         lx_emul/virt/usb_client.c

SRC_CC = main.cc

SRC_CC += lx_emul/event.cc \
          lx_emul/random.cc

SRC_CC += genode_c_api/event.cc \
          genode_c_api/usb_client.cc

vpath %.c  $(PRG_DIR)
vpath %.c  $(PRG_DIR)/../..
vpath %.cc $(PRG_DIR)/../..
vpath %.c  $(REP_DIR)/src/lib
vpath %.cc $(REP_DIR)/src/lib

C_API = $(dir $(call select_from_repositories,src/lib/genode_c_api))

vpath genode_c_api/event.cc      $(C_API)
vpath genode_c_api/usb_client.cc $(C_API)

# vim: set ft=make :
