#
# Specifics for Linux on ARM
#
SPECS += linux arm

REP_INC_DIR += src/platform/arm

ifeq ($(shell gcc -dumpmachine),arm-linux-gnueabihf)
CC_MARCH += -mfloat-abi=hard
endif

#
# We need to manually add the default linker script on the command line in case
# of standard library use. Otherwise, we were not able to extend it by the
# context area section.
#
ifeq ($(USE_HOST_LD_SCRIPT),yes)
LD_SCRIPT_STATIC = ldscripts/armelf_linux_eabi.xc
endif

#
# Include less-specific configuration 
#
include $(call select_from_repositories,mk/spec-arm.mk)
include $(call select_from_repositories,mk/spec-linux.mk)
