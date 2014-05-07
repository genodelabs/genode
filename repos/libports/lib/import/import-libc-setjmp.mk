#
# Add generic libc headers to standard include search paths
#
REP_INC_DIR += include/libc

#
# Add platform-specific libc headers to standard include search paths
#
ifeq ($(filter-out $(SPECS),x86),)
  ifeq ($(filter-out $(SPECS),32bit),)
    LIBC_REP_INC_DIR = include/libc-i386
  endif # 32bit

  ifeq ($(filter-out $(SPECS),64bit),)
    LIBC_REP_INC_DIR = include/libc-amd64
  endif # 32bit
  LIBC_REP_INC_DIR += include/libc-x86
endif # x86

ifeq ($(filter-out $(SPECS),arm),)
  LIBC_REP_INC_DIR = include/libc-arm
endif # ARM

#
# If we found no valid include path for the configured target platform,
# we have to prevent the build system from building the target. This is
# done by adding an artificial requirement.
#
ifeq ($(LIBC_REP_INC_DIR),)
  REQUIRES += libc_support_for_your_target_platform
endif

REP_INC_DIR += $(LIBC_REP_INC_DIR)
