LIBS     += base config
SRC_CC   += serial_driver.cc block_driver.cc vm_base.cc spec/imx53/main.cc
INC_DIR  += $(REP_DIR)/src/server/tz_vmm/spec/imx53
INC_DIR  += $(REP_DIR)/src/server/tz_vmm/include

vpath % $(REP_DIR)/src/server/tz_vmm
