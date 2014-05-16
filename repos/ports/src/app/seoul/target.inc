TARGET = seoul

SEOUL_CONTRIB_DIR = $(call select_from_ports,seoul)/src/app/seoul
SEOUL_GENODE_DIR  = $(SEOUL_CONTRIB_DIR)/genode

REQUIRES += nova

LIBS   += base blit alarm seoul_libc_support config
SRC_CC  = main.cc nova_user_env.cc device_model_registry.cc
SRC_CC += console.cc keyboard.cc network.cc disk.cc
SRC_BIN = mono.tff

MODEL_SRC_CC    += $(notdir $(wildcard $(SEOUL_CONTRIB_DIR)/model/*.cc))
EXECUTOR_SRC_CC += $(notdir $(wildcard $(SEOUL_CONTRIB_DIR)/executor/*.cc))

ifneq ($(filter x86_64, $(SPECS)),)
CC_CXX_OPT += -mcmodel=large
endif

SRC_CC += $(filter-out $(FILTER_OUT),$(addprefix model/,$(MODEL_SRC_CC)))
SRC_CC += $(filter-out $(FILTER_OUT),$(addprefix executor/,$(EXECUTOR_SRC_CC)))

INC_DIR += $(SEOUL_CONTRIB_DIR)/include
INC_DIR += $(SEOUL_GENODE_DIR)/include
INC_DIR += $(PRG_DIR)/../include

CC_WARN += -Wno-parentheses
CC_CXX_OPT += -march=core2
CC_CXX_OPT += -mssse3

vpath %.cc $(SEOUL_CONTRIB_DIR)
vpath %.cc $(PRG_DIR)/..
vpath %.tff $(PRG_DIR)/..
