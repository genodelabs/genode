REQUIRES = linux arm
SRC_S   += lx_clone.S lx_syscall.S

vpath lx_clone.S   $(REP_DIR)/../base-linux/src/platform/arm
vpath lx_syscall.S $(REP_DIR)/../base-linux/src/platform/arm
