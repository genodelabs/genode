TARGET    = tz_vmm
REQUIRES += trustzone imx53
LIBS     += base
LIBS     += config
SRC_CC   += vm.cc
SRC_CC   += main.cc
SRC_CC   += serial.cc
SRC_CC   += block.cc
INC_DIR  += $(PRG_DIR)
INC_DIR  += $(PRG_DIR)/../../include
INC_DIR  += $(PRG_DIR)/../imx53

vpath main.cc $(PRG_DIR)/../imx53
vpath %       $(PRG_DIR)/../..
