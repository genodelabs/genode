TARGET  := sculpt_manager
SRC_CC  := $(notdir $(wildcard $(PRG_DIR)/*.cc))
SRC_CC  += $(addprefix runtime/,$(notdir $(wildcard $(PRG_DIR)/runtime/*.cc)))
SRC_CC  += $(addprefix view/,   $(notdir $(wildcard $(PRG_DIR)/view/*.cc)))
SRC_CC  += $(addprefix model/,  $(notdir $(wildcard $(PRG_DIR)/model/*.cc)))
LIBS    += base
INC_DIR += $(PRG_DIR) $(REP_DIR)/src/app/depot_deploy
