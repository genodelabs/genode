TARGET = vancouver

CONTRIB_DIR = $(REP_DIR)/contrib/vancouver-git
VANCOUVER_DIR = $(CONTRIB_DIR)/vancouver
NOVA_INCLUDE_DIR = $(REP_DIR)/contrib/vancouver-git/base/include

REQUIRES = nova x86_32

ifeq ($(wildcard $(VANCOUVER_DIR)),)
REQUIRES += prepare_ports_vancouver
endif

LIBS  += cxx env blit thread alarm signal server
SRC_CC = main.cc nova_user_env.cc device_model_registry.cc
SRC_CC += console.cc keyboard.cc network.cc disk.cc
SRC_BIN = mono.tff

MODEL_SRC_CC    += $(notdir $(wildcard $(VANCOUVER_DIR)/model/*.cc))
EXECUTOR_SRC_CC += $(notdir $(wildcard $(VANCOUVER_DIR)/executor/*.cc))
SERVICE_SRC_CC  += hostsink.cc

SRC_CC += $(filter-out $(FILTER_OUT),$(addprefix model/,$(MODEL_SRC_CC)))
SRC_CC += $(CONTRIB_DIR)/julian/model/82576vf.cc
SRC_CC += $(filter-out $(FILTER_OUT),$(addprefix executor/,$(EXECUTOR_SRC_CC)))
SRC_CC += base/service/hostsink.cc base/lib/runtime/string.cc

INC_DIR += $(PRG_DIR)
INC_DIR += $(CONTRIB_DIR)/julian/model
INC_DIR += $(CONTRIB_DIR)/julian/include
INC_DIR += $(VANCOUVER_DIR)/model
INC_DIR += $(VANCOUVER_DIR)/executor
INC_DIR += $(VANCOUVER_DIR)/include
INC_DIR += $(NOVA_INCLUDE_DIR)

CC_WARN += -Wno-parentheses
CC_CXX_OPT += -std=gnu++11

LD_TEXT_ADDR = 0xb0000000

vpath %.cc $(VANCOUVER_DIR)
vpath %.cc $(CONTRIB_DIR)
