OKL4_SRC_DIR   = $(call select_from_ports,okl4)/src/kernel/okl4
ARCH_DIR       = $(OKL4_SRC_DIR)/arch/ia32
PLAT_DIR       = $(OKL4_SRC_DIR)/platform/pc99
INC_SYMLINKS   = arch/apic.h \
                 arch/arch_idt.h \
                 arch/asm.h \
                 arch/bootdesc.h \
                 arch/config.h \
                 arch/context.h \
                 arch/cpu.h \
                 arch/fpu.h \
                 arch/hwirq.h \
                 arch/hwspace.h \
                 arch/idt.h \
                 arch/init.h \
                 arch/intctrl.h \
                 arch/interrupt.h \
                 arch/ioport.h \
                 arch/ldt.h \
                 arch/memory.h \
                 arch/mmu.h \
                 arch/offsets.h \
                 arch/pgent.h \
                 arch/platform.h \
                 arch/platsupport.h \
                 arch/profile_asm.h \
                 arch/ptab.h \
                 arch/resource_functions.h \
                 arch/schedule.h \
                 arch/segdesc.h \
                 arch/smp.h \
                 arch/space.h \
                 arch/special.h \
                 arch/syscalls.h \
                 arch/sysdesc.h \
                 arch/tcb.h \
                 arch/timer.h \
                 arch/trapgate.h \
                 arch/traphandler.h \
                 arch/traps.h \
                 arch/tss.h \
                 arch/user_access.h \
                 atomic_ops/arch/atomic_ops.h \
                 cpu/8259.h \
                 cpu/intctrl-pic.h \
                 kernel/arch/cache.h \
                 kernel/arch/config.h \
                 kernel/arch/context.h \
                 kernel/arch/cpu.h \
                 kernel/arch/debug.h \
                 kernel/arch/hwspace.h \
                 kernel/arch/ia32.h \
                 kernel/arch/intctrl.h \
                 kernel/arch/ioport.h \
                 kernel/arch/ktcb.h \
                 kernel/arch/ldt.h \
                 kernel/arch/mmu.h \
                 kernel/arch/offsets.h \
                 kernel/arch/pgent.h \
                 kernel/arch/platform.h \
                 kernel/arch/platsupport.h \
                 kernel/arch/profile.h \
                 kernel/arch/ptab.h \
                 kernel/arch/resource_functions.h \
                 kernel/arch/resources.h \
                 kernel/arch/segdesc.h \
                 kernel/arch/space.h \
                 kernel/arch/special.h \
                 kernel/arch/sync.h \
                 kernel/arch/syscalls.h \
                 kernel/arch/tcb.h \
                 kernel/arch/traceids.h \
                 kernel/arch/tss.h \
                 kernel/arch/types.h \
                 l4/arch/config.h \
                 l4/arch/kdebug.h \
                 l4/arch/specials.h \
                 l4/arch/syscalls.h \
                 l4/arch/thread.h \
                 l4/arch/types.h \
                 l4/arch/vregs.h \
                 l4/ipc.h \
                 l4/kdebug.h \
                 l4/memregion.h \
                 l4/message.h \
                 l4/security.h \
                 l4/thread.h \
                 l4/utcb.h \
                 plat/nmi.h \
                 plat/rtc.h

include $(REP_DIR)/lib/mk/kernel-okl4-include.inc

include/atomic_ops/arch/%.h: $(ARCH_DIR)/libs/atomic_ops/include/%.h
	$(VERBOSE)ln -s $< $@

include/l4/arch/%.h: $(ARCH_DIR)/libs/l4/include/%.h
	$(VERBOSE)ln -s $< $@

include/kernel/arch/%.h: $(ARCH_DIR)/pistachio/include/%.h
	$(VERBOSE)ln -s $< $@

include/arch/%.h: $(ARCH_DIR)/pistachio/include/%.h
	$(VERBOSE)ln -s $< $@

include/cpu/%.h: $(ARCH_DIR)/pistachio/cpu/idt/include/%.h
	$(VERBOSE)ln -s $< $@

include/plat/%.h: $(PLAT_DIR)/pistachio/include/%.h
	$(VERBOSE)ln -s $< $@

