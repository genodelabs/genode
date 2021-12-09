TARGET = seoul
REQUIRES = x86

SEOUL_CONTRIB_DIR = $(call select_from_ports,seoul)/src/app/seoul
SEOUL_GENODE_DIR  = $(SEOUL_CONTRIB_DIR)/genode

LIBS   += base blit seoul_libc_support
SRC_CC  = component.cc user_env.cc device_model_registry.cc state.cc
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
INC_DIR += $(REP_DIR)/src/app/seoul/include
include $(call select_from_repositories,lib/mk/libc-common.inc)

CC_WARN += -Wno-unused

CC_CXX_OPT += -march=core2
CC_OPT_model/intel82576vf := -mssse3
CC_OPT_PIC :=

vpath %.cc  $(SEOUL_CONTRIB_DIR)
vpath %.cc  $(REP_DIR)/src/app/seoul
vpath %.tff $(REP_DIR)/src/app/seoul

CC_CXX_WARN_STRICT_CONVERSION =
