#
# Specifics for Codezero on ARM
#
SPECS += codezero

#
# Linker options specific for ARM
#
LD_TEXT_ADDR ?= 0x02000000

CC_OPT += -D__ARCH__=arm

include $(call select_from_repositories,mk/spec-codezero.mk)
