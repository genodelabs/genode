include $(PRG_DIR)/../target.inc

REQUIRES += arm foc_rpi
SRC_CC   += arm/platform_arm.cc
INC_DIR  += $(REP_DIR)/src/core/include/arm

# add include paths
INC_DIR += $(REP_DIR)/src/core/include/spec/rpi

# configure multiprocessor mode
NR_OF_CPUS = 1

# set address where to link the text segment at
LD_TEXT_ADDR ?= 0x800000

vpath platform_services.cc $(GEN_CORE_DIR)

