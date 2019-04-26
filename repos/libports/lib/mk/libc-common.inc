#
# We use 'select_from_repositories' rather than specifying the patch via
# 'REP_DIR' to enable other repositories to include this file. For example,
# this enables 'ports/lib/mk/seoul_libc_support.mk' to reuse the definitions.
#

LIBC_PORT_DIR := $(call select_from_ports,libc)
LIBC_DIR      := $(LIBC_PORT_DIR)/src/lib/libc
LIBC_REP_DIR  := $(realpath $(call select_from_repositories,include/libc-genode)/../..)

# local libc includes
INC_DIR += $(LIBC_DIR)/lib/libc/locale
INC_DIR += $(LIBC_DIR)/lib/libc/include
INC_DIR += $(LIBC_DIR)/lib/libc/stdio
INC_DIR += $(LIBC_DIR)/lib/libc/net
INC_DIR += $(LIBC_DIR)/contrib/gdtoa

#CC_OPT += -DGENODE_RELEASE

#
# Use default warning level rather than -Wall because we do not want to touch
# the imported source code to improve build aesthetics
#
CC_WARN =
CC_CXX_OPT += -Wall

#
# Generate position independent code to allow linking of static libc code into
# shared libraries (define is evaluated by assembler files)
#
CC_OPT += -DPIC

#
# Disable thread local locales
#
CC_OPT += -D__NO_TLS

#
# Use libc import rules also for building the libc itself
#
include $(call select_from_repositories,lib/import/import-libc.mk)
