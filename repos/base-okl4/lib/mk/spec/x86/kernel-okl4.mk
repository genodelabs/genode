OKL4_SRC_DIR   = $(call select_from_ports,okl4)/src/kernel/okl4

CONFIG         = ARCH_IA32 \
                 CONFIG_ARCH_IA32=1 \
                 CONFIG_CPU_IA32_I686 \
                 CONFIG_CPU_IA32_I686 \
                 CONFIG_CPU_IA32_IDT=1 \
                 CONFIG_KDB_CONS_COM=1 \
                 CONFIG_KDB_CONS_SERIAL=1 \
                 CONFIG_PLAT_PC99=1 \
                 CONFIG_ROOT_CAP_BITS=12 \
                 ENDIAN_LITTLE \
                 L4_ARCH_IA32 \
                 MACHINE_IA32_PC99 \
                 __ARCH__=ia32 \
                 __CPU__=idt \
                 __L4_ARCH__=ia32 \
                 __PLATFORM__=pc99
SRC_CC         = macro_sets.cc \
                 arch/ia32/pistachio/kdb/breakpoints.cc \
                 arch/ia32/pistachio/kdb/stepping.cc \
                 arch/ia32/pistachio/kdb/x86.cc \
                 arch/ia32/pistachio/kdb/glue/prepost.cc \
                 arch/ia32/pistachio/kdb/glue/readmem.cc \
                 arch/ia32/pistachio/kdb/glue/resources.cc \
                 arch/ia32/pistachio/kdb/glue/space.cc \
                 arch/ia32/pistachio/kdb/glue/thread.cc \
                 arch/ia32/pistachio/src/debug.cc \
                 arch/ia32/pistachio/src/exception.cc \
                 arch/ia32/pistachio/src/init.cc \
                 arch/ia32/pistachio/src/ldt.cc \
                 arch/ia32/pistachio/src/platsupport.cc \
                 arch/ia32/pistachio/src/resources.cc \
                 arch/ia32/pistachio/src/schedule.cc \
                 arch/ia32/pistachio/src/smp.cc \
                 arch/ia32/pistachio/src/space.cc \
                 arch/ia32/pistachio/src/thread.cc \
                 arch/ia32/pistachio/src/user.cc \
                 platform/pc99/pistachio/src/plat.cc \
                 platform/pc99/pistachio/src/reboot.cc
SRC_CC        += $(subst $(OKL4_SRC_DIR)/,,$(wildcard $(OKL4_SRC_DIR)/arch/ia32/pistachio/cpu/idt/src/*.cc)) \
                 $(subst $(OKL4_SRC_DIR)/,,$(wildcard $(OKL4_SRC_DIR)/platform/pc99/pistachio/kdb/*.cc))
SRC_SPP        = arch/ia32/pistachio/src/trap.spp \
                 arch/ia32/pistachio/src/gnu/bootmem-elf.spp \
                 platform/pc99/pistachio/src/smp.spp \
                 platform/pc99/pistachio/src/startup.spp
LD_TEXT_ADDR  := 0xf0100000

-include $(REP_DIR)/lib/mk/kernel-okl4.inc

LD_SCRIPT_STATIC = $(REP_DIR)/contrib/generated/x86/linker.ld
INC_DIR          = include \
                   $(REP_DIR)/contrib/generated/x86 \
                   $(OKL4_SRC_DIR)/pistachio/include

vpath macro_sets.cc $(REP_DIR)/contrib/generated/x86
vpath %.spp         $(OKL4_SRC_DIR)/arch/ia32/pistachio/src
