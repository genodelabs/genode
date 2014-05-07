TARGET = vancouver

CONTRIB_DIR = $(REP_DIR)/contrib/seoul-git
VANCOUVER_DIR = $(CONTRIB_DIR)
GENODE_DIR = $(CONTRIB_DIR)/genode
NOVA_INCLUDE_DIR = $(REP_DIR)/contrib/seoul-git/include

REQUIRES += nova

ifeq ($(wildcard $(VANCOUVER_DIR)),)
REQUIRES += prepare_ports_vancouver
endif

LIBS   += base blit alarm seoul_libc_support config
SRC_CC  = main.cc nova_user_env.cc device_model_registry.cc
SRC_CC += console.cc keyboard.cc network.cc disk.cc
SRC_BIN = mono.tff

MODEL_SRC_CC    += $(notdir $(wildcard $(VANCOUVER_DIR)/model/*.cc))
EXECUTOR_SRC_CC += $(notdir $(wildcard $(VANCOUVER_DIR)/executor/*.cc))
SERVICE_SRC_CC  += sink.cc

ifneq ($(filter x86_64, $(SPECS)),)
CC_CXX_OPT += -mcmodel=large
endif

SRC_CC += $(filter-out $(FILTER_OUT),$(addprefix model/,$(MODEL_SRC_CC)))
SRC_CC += $(filter-out $(FILTER_OUT),$(addprefix executor/,$(EXECUTOR_SRC_CC)))
SRC_CC += model/sink.cc

INC_DIR += $(PRG_DIR)/..
INC_DIR += $(PRG_DIR)/../executor
INC_DIR += $(VANCOUVER_DIR)/model
INC_DIR += $(VANCOUVER_DIR)/executor
INC_DIR += $(VANCOUVER_DIR)/include $(GENODE_DIR)/include
INC_DIR += $(NOVA_INCLUDE_DIR)

CC_WARN += -Wno-parentheses
CC_CXX_OPT += -march=core2
CC_CXX_OPT += -mssse3

vpath %.cc $(VANCOUVER_DIR)
vpath %.cc $(CONTRIB_DIR)
vpath %.cc $(PRG_DIR)/..
vpath %.tff $(PRG_DIR)/..
