INC_SYMLINKS  += atomic_ops/atomic_ops.h \
                 atomic_ops/unsafe_generic.h \
                 compat/c.h \
                 compat/toolchain/ads/c.h \
                 compat/toolchain/flint/c.h \
                 compat/toolchain/gnu/c.h \
                 compat/toolchain/rvct/c.h \
                 compat/toolchain/rvct_gnu/c.h \
                 kdb/cmd.h \
                 kdb/console.h \
                 kdb/init.h \
                 kdb/input.h \
                 kdb/kdb.h \
                 kdb/macro_set.h \
                 kdb/print.h \
                 kdb/tid_format.h \
                 kdb/tracepoints.h \
                 kernel/bitmap.h \
                 kernel/bitmask.h \
                 kernel/cache.h \
                 kernel/caps.h \
                 kernel/clist.h \
                 kernel/config.h \
                 kernel/debug.h \
                 kernel/endpoint.h \
                 kernel/fpage.h \
                 kernel/generic/lib.h \
                 kernel/idtable.h \
                 kernel/init.h \
                 kernel/interrupt.h \
                 kernel/ipc.h \
                 kernel/kdb/console.h \
                 kernel/kdb/macro_set.h \
                 kernel/kdb/names.h \
                 kernel/kdb/tracepoints.h \
                 kernel/kmemory.h \
                 kernel/l4.h \
                 kernel/macros.h \
                 kernel/map.h \
                 kernel/memdesc.h \
                 kernel/mp.h \
                 kernel/mutex.h \
                 kernel/mutexid.h \
                 kernel/platform.h \
                 kernel/preempt.h \
                 kernel/profile.h \
                 kernel/queueing.h \
                 kernel/read_write_lock.h \
                 kernel/resources.h \
                 kernel/rootserver.h \
                 kernel/schedule.h \
                 kernel/smallalloc.h \
                 kernel/space.h \
                 kernel/spaceid.h \
                 kernel/sync.h \
                 kernel/syncpoint.h \
                 kernel/syscalls.h \
                 kernel/tcb.h \
                 kernel/tcb_syscall_data.h \
                 kernel/thread.h \
                 kernel/threadstate.h \
                 kernel/tracebuffer.h \
                 kernel/traceids.h \
                 kernel/types.h \
                 kernel/utcb.h \
                 l4/config.h \
                 l4/macros.h \
                 l4/types.h

SYMLINK_TARGETS = $(addprefix include/,$(INC_SYMLINKS))
SYMLINK_DIRS    = $(sort $(dir $(SYMLINK_TARGETS)))

ifeq ($(called_from_lib_mk),yes)
all: $(SYMLINK_TARGETS)
endif

$(SYMLINK_TARGETS): $(filter-out $(wildcard $(SYMLINK_DIRS)), $(SYMLINK_DIRS))

$(SYMLINK_DIRS):
	$(VERBOSE)mkdir -p $@

include/kernel/kdb/%.h: $(OKL4_SRC_DIR)/pistachio/kdb/include/%.h
	$(VERBOSE)ln -s $< $@

include/kdb/%.h: $(OKL4_SRC_DIR)/pistachio/kdb/include/%.h
	$(VERBOSE)ln -s $< $@

include/kernel/%.h: $(OKL4_SRC_DIR)/pistachio/include/%.h
	$(VERBOSE)ln -s $< $@

include/compat/%.h: $(OKL4_SRC_DIR)/libs/compat/include/compat/%.h
	$(VERBOSE)ln -s $< $@

include/atomic_ops/%.h: $(OKL4_SRC_DIR)/libs/atomic_ops/include/%.h
	$(VERBOSE)ln -s $< $@

include/l4/%.h: $(OKL4_SRC_DIR)/libs/l4/include/%.h
	$(VERBOSE)ln -s $< $@

