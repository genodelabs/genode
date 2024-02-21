TARGET  := phone_manager

SCULPT_MANAGER_DIR := $(call select_from_repositories,src/app/sculpt_manager)
DEPOT_DEPLOY_DIR   := $(call select_from_repositories,src/app/depot_deploy)

SRC_CC  += $(notdir $(wildcard $(PRG_DIR)/*.cc))
SRC_CC  += $(addprefix view/,    $(notdir $(wildcard $(SCULPT_MANAGER_DIR)/view/*.cc)))
SRC_CC  += $(addprefix runtime/, $(notdir $(wildcard $(SCULPT_MANAGER_DIR)/runtime/*.cc)))
SRC_CC  += $(addprefix dialog/,  $(notdir $(wildcard $(SCULPT_MANAGER_DIR)/dialog/*.cc)))
SRC_CC  += gui.cc graph.cc deploy.cc storage.cc network.cc
LIBS    += base
INC_DIR += $(PRG_DIR) $(SCULPT_MANAGER_DIR) $(DEPOT_DEPLOY_DIR)

vpath %.cc $(PRG_DIR)
vpath %.cc $(SCULPT_MANAGER_DIR)
