REQUIRES := x86

REL_PRG_DIR := $(PRG_DIR)/../..

TARGET  := pc_usb_host
LIBS    := base pc_lx_emul jitterentropy

INC_DIR += $(REL_PRG_DIR)
SRC_CC  += main.cc
SRC_CC  += lx_emul/shared_dma_buffer.cc
SRC_C   += dummies.c
SRC_C   += lx_emul.c
SRC_C   += $(notdir $(wildcard $(REL_PRG_DIR)/generated_dummies.c))
SRC_C   += common_dummies.c
SRC_C   += lx_emul/usb.c

SRC_C   += lx_emul/shadow/lib/kobject_uevent.c
vpath %.c  $(REP_DIR)/src/lib/pc
vpath %.cc $(REP_DIR)/src/lib/pc

vpath %.c $(REL_PRG_DIR)
vpath %.cc $(REL_PRG_DIR)
vpath common_dummies.c $(REP_DIR)/src/lib/pc/lx_emul

#
# Genode C-API backends
#

SRC_CC  += genode_c_api/usb.cc

vpath genode_c_api/usb.cc $(subst /genode_c_api,,$(call select_from_repositories,src/lib/genode_c_api))
