PROGRAM_PREFIX = genode-x86-
GCC_TARGET     = x86_64-pc-elf

# cross-compiling does not work yet
REQUIRES = x86

include $(PRG_DIR)/../gcc/target.inc
