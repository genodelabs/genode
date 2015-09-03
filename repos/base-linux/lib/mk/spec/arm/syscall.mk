REQUIRES = linux arm
SRC_S   += lx_clone.S lx_syscall.S

vpath lx_clone.S   $(REP_DIR)/src/lib/syscall/spec/arm
vpath lx_syscall.S $(REP_DIR)/src/lib/syscall/spec/arm
