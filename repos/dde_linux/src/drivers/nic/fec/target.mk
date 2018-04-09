REQUIRES = arm_v7

TARGET   = nic_drv
LIBS     = base lx_kit_setjmp fec_nic_include
SRC_CC   = main.cc platform.cc lx_emul.cc component.cc
SRC_C   += dummy.c lxc.c
INC_DIR += $(PRG_DIR)

# lx_kit
SRC_CC  += env.cc irq.cc malloc.cc scheduler.cc timer.cc work.cc printf.cc
INC_DIR += $(REP_DIR)/src/include
INC_DIR += $(REP_DIR)/src/include/spec/arm
INC_DIR += $(REP_DIR)/src/include/spec/arm_v7
INC_DIR += $(REP_DIR)/src/lib/usb/include/spec/arm

# contrib code
LX_CONTRIB_DIR := $(call select_from_ports,dde_linux)/src/drivers/nic/fec
SRC_C          += $(notdir $(wildcard $(LX_CONTRIB_DIR)/drivers/net/ethernet/freescale/*.c))
SRC_C          += $(notdir $(wildcard $(LX_CONTRIB_DIR)/drivers/net/phy/*.c))
SRC_C          += $(notdir $(wildcard $(LX_CONTRIB_DIR)/net/core/*.c))
SRC_C          += $(notdir $(wildcard $(LX_CONTRIB_DIR)/net/ethernet/*.c))
INC_DIR        += $(LX_CONTRIB_DIR)/include

#
# Linux sources are C89 with GNU extensions
#
CC_C_OPT += -std=gnu89

#
# Reduce build noise of compiling contrib code
#
CC_OPT_fec_ptp  = -Wno-unused-but-set-variable -Wno-unused-variable \
                  -Wno-maybe-uninitialized -Wno-uninitialized
CC_OPT_fec_main = -Wno-unused-but-set-variable -Wno-unused-variable \
                  -Wno-pointer-sign -Wno-int-conversion -Wno-unused-function \
                  -Wno-uninitialized
CC_OPT_skbuff   = -Wno-pointer-sign -Wno-int-conversion -Wno-uninitialized
CC_OPT_mdio_bus = -Wno-implicit-int -Wno-unused-function -Wno-pointer-sign
CC_OPT_eth      = -Wno-pointer-sign -Wno-unused-function
CC_OPT_phy      = -Wno-unused-function -Wno-unused-but-set-variable
CC_OPT_at803x   = -Wno-unused-variable

vpath %.c  $(LX_CONTRIB_DIR)/drivers/net/ethernet/freescale
vpath %.c  $(LX_CONTRIB_DIR)/drivers/net/phy
vpath %.c  $(LX_CONTRIB_DIR)/net/core
vpath %.c  $(LX_CONTRIB_DIR)/net/ethernet
vpath %.cc $(PRG_DIR)
vpath %.cc $(REP_DIR)/src/lx_kit

CC_CXX_WARN_STRICT =
