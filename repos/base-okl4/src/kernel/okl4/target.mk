TARGET = okl4
LIBS   = kernel-okl4
SRC_C  = dummy.c

#
# Prevent the kernel binary from being stripped. Otherwise, elfweaver would
# complain with the following error:
#
# Error: Symbol "tcb_size" not found in kernel ELF file.  Needed for XIP support.
#
STRIP_TARGET_CMD = cp -f $< $@

LD_TEXT_ADDR    := 0xf0100000
LD_SCRIPT_STATIC = $(REP_DIR)/contrib/generated/x86/linker.ld

$(TARGET): dummy.c
dummy.c:
	@touch $@
