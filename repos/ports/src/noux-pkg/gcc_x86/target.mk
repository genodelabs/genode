PROGRAM_PREFIX = genode-x86-
GCC_TARGET     = x86_64-pc-elf

# cross-compiling does not work yet
REQUIRES = x86

MAKE_ENV += MULTILIB_OPTIONS="m64/m32" MULTILIB_DIRNAMES="64 32"

include $(PRG_DIR)/../gcc/target.inc
