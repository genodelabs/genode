#
# Specifics for the NOVA kernel API x86 64 bit
#

SPECS += nova x86_64

include $(call select_from_repositories,mk/spec-x86_64.mk)
include $(call select_from_repositories,mk/spec-nova.mk)
