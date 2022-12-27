ifeq ($(filter-out $(SPECS),x86_32),)
	TARGET_CPUARCH=x86_32
else ifeq ($(filter-out $(SPECS),x86_64),)
	TARGET_CPUARCH=x86_64
else ifeq ($(filter-out $(SPECS),arm_v6),)
	TARGET_CPUARCH=arm_v6
else ifeq ($(filter-out $(SPECS),arm_v7),)
	TARGET_CPUARCH=arm_v7
else ifeq ($(filter-out $(SPECS),arm_v8),)
	TARGET_CPUARCH=arm_v8
endif

JITTERENTROPY_DIR := $(call select_from_ports,jitterentropy)

ifeq ($(CONTRIB_DIR),)
REP_INC_DIR += include/jitterentropy
else
INC_DIR += $(JITTERENTROPY_DIR)/include/jitterentropy
endif

INC_DIR += $(call select_from_repositories,src/lib/jitterentropy)
INC_DIR += $(call select_from_repositories,src/lib/jitterentropy/spec/$(TARGET_CPUARCH))

CC_OPT += -DJITTERENTROPY_GENODE
