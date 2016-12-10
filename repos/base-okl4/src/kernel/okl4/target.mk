TARGET = okl4
LIBS   = kernel-okl4
SRC_C  = dummy.c

LD_TEXT_ADDR    := 0xf0100000
LD_SCRIPT_STATIC = $(REP_DIR)/contrib/generated/x86/linker.ld

$(TARGET): dummy.c

dummy.c:
	@touch $@
