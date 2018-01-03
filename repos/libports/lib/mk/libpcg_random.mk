include $(REP_DIR)/lib/import/import-libpcg_random.mk
PCG_SRC_DIR = $(call select_from_ports,pcg-c)/src/lib/pcg-c/src

CC_OPT += -std=c99

pcg_srcs = $(notdir $(wildcard $(PCG_SRC_DIR)/*-$(1).c))

SRC_C += $(call pcg_srcs,8)
SRC_C += $(call pcg_srcs,16)
SRC_C += $(call pcg_srcs,32)
SRC_C += $(call pcg_srcs,64)

# 128bit state generators only available on 64bit platforms
ifeq ($(filter-out $(SPECS),64bit),)
	SRC_C += $(call pcg_srcs,128)
endif

vpath %.c $(PCG_SRC_DIR)

CC_CXX_WARN_STRICT =
