HOST_INC_DIR += $(dir $(call select_from_repositories,src/platform/linux_syscalls.h))
HOST_INC_DIR += /usr/include

# needed for Ubuntu 11.04
HOST_INC_DIR += /usr/include/i386-linux-gnu
HOST_INC_DIR += /usr/include/x86_64-linux-gnu
