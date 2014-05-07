#
# Specifics for Linux on 64-bit x86
#
SPECS += linux x86_64

REP_INC_DIR += src/platform/x86_64

#
# We need to manually add the default linker script on the command line in case
# of standard library use. Otherwise, we were not able to extend it by the
# context area section.
#
ifeq ($(USE_HOST_LD_SCRIPT),yes)
LD_SCRIPT_DEFAULT = ldscripts/elf_x86_64.xc
endif

#
# Include less-specific configuration 
#
include $(call select_from_repositories,mk/spec-x86_64.mk)
include $(call select_from_repositories,mk/spec-linux.mk)

