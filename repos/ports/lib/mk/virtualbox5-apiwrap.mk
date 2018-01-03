include $(REP_DIR)/lib/mk/virtualbox5-common.inc

LIBS  += stdcxx

SRC_CC += $(addprefix VBoxAPIWrap/, $(notdir $(wildcard $(VIRTUALBOX_DIR)/VBoxAPIWrap/*.cpp)))

INC_DIR += $(REP_DIR)/src/virtualbox5/frontend
INC_DIR += $(VBOX_DIR)/Main/include

vpath %.cpp $(VIRTUALBOX_DIR)

CC_CXX_WARN_STRICT =
