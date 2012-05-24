LIBC_DIR = $(REP_DIR)/contrib/libc-8.2.0

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
include $(REP_DIR)/lib/import/import-libc.mk
