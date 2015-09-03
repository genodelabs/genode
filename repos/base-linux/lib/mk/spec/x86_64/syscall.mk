REQUIRES = linux x86
SRC_S   += lx_clone.S lx_restore_rt.S lx_syscall.S

vpath lx_restore_rt.S $(REP_DIR)/src/lib/syscall/spec/x86_64
vpath lx_clone.S      $(REP_DIR)/src/lib/syscall/spec/x86_64
vpath lx_syscall.S    $(REP_DIR)/src/lib/syscall/spec/x86_64

