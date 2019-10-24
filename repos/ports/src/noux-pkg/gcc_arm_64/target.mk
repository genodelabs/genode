PROGRAM_PREFIX = genode-aarch64-
GCC_TARGET     = aarch64-none-elf

# cross-compiling does not work yet
REQUIRES = arm_64

include $(PRG_DIR)/../gcc/target.inc
