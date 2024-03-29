PROGRAM_PREFIX = genode-arm-
GDB_TARGET     = arm-none-eabi

#
# We link libraries to the final binaries using the 'LIBS' variable.  But
# unfortunately, the gdb build system has hardcoded some libs such as '-lm'.
# To satisfy the linker, we provide dummy archives.
#

LDFLAGS += -L$(PWD)

include $(PRG_DIR)/../gdb/target.inc
