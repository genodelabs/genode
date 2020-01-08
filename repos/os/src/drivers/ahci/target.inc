SRC_CC  += main.cc platform.cc
INC_DIR += $(REP_DIR)/src/drivers/ahci
LIBS    += base

vpath platform.cc $(PRG_DIR)
vpath %.cc        $(REP_DIR)/src/drivers/ahci
