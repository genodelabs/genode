#
# We use 'select_from_repositories' rather than specifying the patch via
# 'REP_DIR' to enable other repositories to include this file. For example,
# this enables 'ports/lib/mk/seoul_libc_support.mk' to reuse the definitions.
#

LIBC_DIR     = $(call select_from_repositories,contrib/libc-8.2.0)
LIBC_REP_DIR = $(realpath $(subst /contrib/libc-8.2.0,,$(LIBC_DIR)))

# local libc includes
INC_DIR += $(LIBC_DIR)/libc/locale
INC_DIR += $(LIBC_DIR)/libc/include
INC_DIR += $(LIBC_DIR)/libc/stdio
INC_DIR += $(LIBC_DIR)/libc/net
INC_DIR += $(LIBC_DIR)/gdtoa

#CC_OPT += -DGENODE_RELEASE

#
# Use default warning level rather than -Wall because we do not want to touch
# the imported source code to improve build aesthetics
#
CC_WARN =

#
# Generate position independent code to allow linking of static libc code into
# shared libraries (define is evaluated by assembler files)
#
CC_OPT += -DPIC

#
# Use libc import rules also for building the libc itself
#
include $(call select_from_repositories,lib/import/import-libc.mk)
