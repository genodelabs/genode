TARGET      = audio_out_drv
REQUIRES    = x86_32
LIBS        = dde_kit
OSS_DIR    := $(call select_from_ports,oss)/src/lib/oss



CC_OPT += -Ulinux -D_KERNEL -fno-omit-frame-pointer

#
# Silence C code
#
CC_C_OPT = -Wno-unused-variable -Wno-unused-but-set-variable \
           -Wno-implicit-function-declaration \
           -Wno-sign-compare

#
# Native Genode sources
#
SRC_CC = os.cc main.cc driver.cc irq.cc quirks.cc
SRC_C  = dummies.c

#
# Driver sources
#
DRV = $(addprefix $(OSS_DIR)/,kernel/drv target)

#
# Framwork sources
#
FRAMEWORK = $(addprefix $(OSS_DIR)/kernel/framework/,\
              osscore audio mixer vmix_core midi ac97)

# find C files
SRC_C += $(shell find $(DRV) $(FRAMEWORK)  -name *.c -exec basename {} ";")

# find directories for VPATH
PATHS  = $(shell find $(DRV) $(FRAMEWORK) -type d)

# add include directories
INC_DIR += $(OSS_DIR)/kernel/framework/include $(OSS_DIR)/include \
           $(PRG_DIR)/include $(PRG_DIR)


vpath %.cc $(PRG_DIR)/signal
vpath %.c  $(PATHS)
