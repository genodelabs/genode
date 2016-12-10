REQUIRES = linux x86
SRC_S   += lx_clone.S lx_syscall.S

vpath lx_clone.S   $(REP_DIR)/src/lib/syscall/spec/x86_32
vpath lx_syscall.S $(REP_DIR)/src/lib/syscall/spec/x86_32
