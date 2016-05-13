#
# Add platform-specific libc headers to standard include search paths
#
ifeq ($(filter-out $(SPECS),x86),)
  ifeq ($(filter-out $(SPECS),32bit),)
    LIBC_ARCH_INC_DIR := include/libc-i386
  endif # 32bit

  ifeq ($(filter-out $(SPECS),64bit),)
    LIBC_ARCH_INC_DIR := include/libc-amd64
  endif # 64bit
endif # x86

ifeq ($(filter-out $(SPECS),arm),)
  LIBC_ARCH_INC_DIR := include/libc-arm
endif # ARM

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
else
LIBC_PORT_DIR := $(call select_from_ports,libc)
INC_DIR += $(LIBC_PORT_DIR)/include/libc
INC_DIR += $(LIBC_PORT_DIR)/$(LIBC_ARCH_INC_DIR)
endif

#
# Genode-specific supplements to standard libc headers
#
REP_INC_DIR += include/libc-genode

#
# Prevent gcc headers from defining __size_t. This definition is done in
# machine/_types.h.
#
CC_OPT += -D__FreeBSD__=8

#
# Provide C99 API functions (needed for C++11 in stdcxx at least)
#
CC_OPT += -D__ISO_C_VISIBLE=1999

#
# Prevent gcc-4.4.5 from generating code for the family of 'sin' and 'cos'
# functions because the gcc-generated code would actually call 'sincos'
# or 'sincosf', which is a GNU extension, not provided by our libc.
#
CC_OPT += -fno-builtin-sin -fno-builtin-cos -fno-builtin-sinf -fno-builtin-cosf
