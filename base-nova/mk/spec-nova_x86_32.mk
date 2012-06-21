#
# Specifics for the NOVA kernel API x86 32 bit
#

SPECS += nova x86_32

include $(call select_from_repositories,mk/spec-x86_32.mk)
include $(call select_from_repositories,mk/spec-nova.mk)
