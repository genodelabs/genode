#
# Add platform-specific libc headers to standard include search paths
#
ifeq ($(filter-out $(SPECS),x86_32),)
	LIBC_ARCH_INC_DIR := include/spec/x86_32/libc
endif # x86_32

ifeq ($(filter-out $(SPECS),x86_64),)
	LIBC_ARCH_INC_DIR := include/spec/x86_64/libc
endif # x86_64

ifeq ($(filter-out $(SPECS),arm),)
	CC_OPT += -D__ARM_PCS_VFP
	LIBC_ARCH_INC_DIR := include/spec/arm/libc
endif # ARM

ifeq ($(filter-out $(SPECS),arm_64),)
	LIBC_ARCH_INC_DIR := include/spec/arm_64/libc
endif # ARM64

#
# If we found no valid include path for the configured target platform,
# we have to prevent the build system from building the target. This is
# done by adding an artificial requirement.
#
ifeq ($(LIBC_ARCH_INC_DIR),)
  REQUIRES += libc_support_for_your_target_platform
endif

ifeq ($(CONTRIB_DIR),)
REP_INC_DIR += include/libc
REP_INC_DIR += $(LIBC_ARCH_INC_DIR)
ifeq ($(filter-out $(SPECS),x86),)
	REP_INC_DIR += include/spec/x86/libc
endif
else
LIBC_PORT_DIR := $(call select_from_ports,libc)
INC_DIR += $(LIBC_PORT_DIR)/include/libc
INC_DIR += $(LIBC_PORT_DIR)/$(LIBC_ARCH_INC_DIR)
ifeq ($(filter-out $(SPECS),x86),)
	INC_DIR += $(LIBC_PORT_DIR)/include/spec/x86/libc
endif
endif

#
# Genode-specific supplements to standard libc headers
#
REP_INC_DIR += include/libc-genode

#
# Prevent gcc headers from defining __size_t. This definition is done in
# machine/_types.h.
#
CC_OPT += -D__FreeBSD__=12

#
# Prevent gcc-4.4.5 from generating code for the family of 'sin' and 'cos'
# functions because the gcc-generated code would actually call 'sincos'
# or 'sincosf', which is a GNU extension, not provided by our libc.
#
CC_OPT += -fno-builtin-sin -fno-builtin-cos -fno-builtin-sinf -fno-builtin-cosf

CC_OPT += -D__GENODE__
