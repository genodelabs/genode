TARGET  := sculpt_manager
SRC_CC  := $(notdir $(wildcard $(PRG_DIR)/*.cc))
SRC_CC  += $(addprefix view/, $(notdir $(wildcard $(PRG_DIR)/view/*.cc)))
LIBS    += base
INC_DIR += $(PRG_DIR) $(REP_DIR)/src/app/depot_deploy
REQUIRES = x86
