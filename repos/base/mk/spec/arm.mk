#
# ARM-specific Genode headers
#
REP_INC_DIR += include/spec/arm

SPECS += 32bit

#
# Prevent compiler message
# "note: the mangling of 'va_list' has changed in GCC 4.4"
#
CC_OPT += -Wno-psabi

#
# By default, the linker produces ELF binaries where the test segment is
# aligned to the largest page size. On ARM this is 64K. As a result, the
# produced binaries contain almost 64K zeros preceding the text segment.
# The patch enforces the text segment to be aligned to 4K instead.
#
LD_MARCH += -z max-page-size=0x1000

include $(call select_from_repositories,mk/spec/32bit.mk)
