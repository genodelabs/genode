REQUIRES = linux x86
SRC_S   += lx_clone.S lx_syscall.S

vpath lx_clone.S   $(REP_DIR)/../base-linux/src/platform/x86_32
vpath lx_syscall.S $(REP_DIR)/../base-linux/src/platform/x86_32
