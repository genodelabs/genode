#
# Content hosted in the dde_linux repository
#

MIRRORED_FROM_DDE_LINUX := src/lib/lx_emul \
                           src/lib/lx_kit \
                           src/include/lx_emul \
                           src/include/lx_user \
                           src/include/spec/x86_32 \
                           src/include/spec/x86_64 \
                           src/include/lx_kit \
                           lib/import/import-lx_emul_common.inc

content: $(MIRRORED_FROM_DDE_LINUX)
$(MIRRORED_FROM_DDE_LINUX):
	mkdir -p $(dir $@); cp -r $(GENODE_DIR)/repos/dde_linux/$@ $(dir $@)


#
# Content hosted in the pc repository
#

MIRRORED_FROM_REP_DIR := lib/mk/pc_linux_generated.inc \
                         lib/mk/pc_lx_emul.mk \
                         lib/mk/spec/x86_64/pc_linux_generated.mk \
                         lib/mk/spec/x86_32/pc_linux_generated.mk \
                         lib/import/import-pc_lx_emul.mk \
                         src/pc_linux/target.inc

content: $(MIRRORED_FROM_REP_DIR)
$(MIRRORED_FROM_REP_DIR):
	$(mirror_from_rep_dir)


#
# Content from the Linux source tree
#

PORT_DIR := $(call port_dir,$(GENODE_DIR)/repos/dde_linux/ports/linux)
LX_REL_DIR := src/linux
LX_ABS_DIR := $(addsuffix /$(LX_REL_DIR),$(PORT_DIR))

# ingredients needed for creating a Linux build directory / generated headers
LX_FILES += Kbuild \
            Makefile \
            arch/x86/Makefile \
            arch/x86/Makefile_32.cpu \
            arch/x86/configs \
            arch/x86/entry/syscalls/Makefile \
            arch/x86/entry/syscalls/syscall_32.tbl \
            arch/x86/entry/syscalls/syscall_64.tbl \
            arch/x86/include/asm/Kbuild \
            arch/x86/include/asm/atomic64_32.h \
            arch/x86/include/asm/cmpxchg_32.h \
            arch/x86/include/asm/string.h \
            arch/x86/include/asm/string_32.h \
            arch/x86/include/asm/string_64.h \
            arch/x86/include/uapi/asm/Kbuild \
            arch/x86/include/uapi/asm/posix_types.h \
            arch/x86/include/uapi/asm/posix_types_32.h \
            arch/x86/include/uapi/asm/posix_types_64.h \
            arch/x86/tools/Makefile \
            arch/x86/tools/relocs.c \
            arch/x86/tools/relocs.h \
            arch/x86/tools/relocs_32.c \
            arch/x86/tools/relocs_64.c \
            arch/x86/tools/relocs_common.c \
            include/asm-generic/bitops/fls64.h \
            include/asm-generic/Kbuild \
            include/linux/compiler-version.h \
            include/linux/kbuild.h \
            include/linux/license.h \
            include/uapi/Kbuild \
            include/uapi/asm-generic/Kbuild \
            kernel/configs/tiny-base.config \
            kernel/configs/tiny.config \
            scripts/Kbuild.include \
            scripts/Makefile \
            scripts/Makefile.asm-generic \
            scripts/Makefile.build \
            scripts/Makefile.compiler \
            scripts/Makefile.extrawarn \
            scripts/Makefile.host \
            scripts/Makefile.lib \
            scripts/asn1_compiler.c \
            scripts/as-version.sh \
            scripts/basic/Makefile \
            scripts/basic/fixdep.c \
            scripts/cc-version.sh \
            scripts/check-local-export \
            scripts/checksyscalls.sh \
            scripts/config \
            scripts/dtc \
            scripts/kconfig/merge_config.sh \
            scripts/ld-version.sh \
            scripts/mkcompile_h \
            scripts/min-tool-version.sh \
            scripts/mod \
            scripts/pahole-flags.sh \
            scripts/pahole-version.sh \
            scripts/remove-stale-files \
            scripts/setlocalversion \
            scripts/sorttable.c \
            scripts/sorttable.h \
            scripts/subarch.include \
            scripts/syscallhdr.sh \
            scripts/syscalltbl.sh \
            tools/include/tools \
            tools/objtool

# needed for src/asn1_compiler.c
LX_FILES += include/linux/asn1.h \
            include/linux/asn1_ber_bytecode.h

LX_SCRIPTS_KCONFIG_FILES := $(notdir $(wildcard $(LX_ABS_DIR)/scripts/kconfig/*.c)) \
                            $(notdir $(wildcard $(LX_ABS_DIR)/scripts/kconfig/*.h)) \
                            Makefile lexer.l parser.y
LX_FILES += $(addprefix scripts/kconfig/,$(LX_SCRIPTS_KCONFIG_FILES)) \

LX_FILES += $(shell cd $(LX_ABS_DIR); find -name "Kconfig*" -printf "%P\n")

# needed for generated/asm-offsets.h
LX_FILES += arch/x86/include/asm/boot.h \
            arch/x86/include/asm/coco.h \
            arch/x86/include/asm/cpufeature.h \
            arch/x86/include/asm/current.h \
            arch/x86/include/asm/fixmap.h \
            arch/x86/include/asm/fpu/api.h \
            arch/x86/include/asm/fpu/xstate.h \
            arch/x86/include/asm/ia32.h \
            arch/x86/include/asm/irqflags.h \
            arch/x86/include/asm/nospec-branch.h \
            arch/x86/include/asm/page.h \
            arch/x86/include/asm/page_32.h \
            arch/x86/include/asm/page_32_types.h \
            arch/x86/include/asm/page_64.h \
            arch/x86/include/asm/pgtable.h \
            arch/x86/include/asm/pgtable-2level.h \
            arch/x86/include/asm/pgtable-2level_types.h \
            arch/x86/include/asm/pgtable_32.h \
            arch/x86/include/asm/pgtable_32_areas.h \
            arch/x86/include/asm/pgtable_32_types.h \
            arch/x86/include/asm/pgtable_64.h \
            arch/x86/include/asm/pgtable-invert.h \
            arch/x86/include/asm/pkru.h \
            arch/x86/include/asm/qrwlock.h \
            arch/x86/include/asm/qspinlock.h \
            arch/x86/include/asm/sigframe.h \
            arch/x86/include/asm/special_insns.h \
            arch/x86/include/asm/spinlock.h \
            arch/x86/include/asm/suspend.h \
            arch/x86/include/asm/suspend_32.h \
            arch/x86/include/asm/suspend_64.h \
            arch/x86/include/asm/sync_core.h \
            arch/x86/include/asm/uaccess_32.h \
            arch/x86/include/asm/uaccess_64.h \
            arch/x86/include/asm/user_32.h \
            arch/x86/include/uapi/asm/ucontext.h \
            arch/x86/kernel/asm-offsets.c \
            arch/x86/kernel/asm-offsets_32.c \
            arch/x86/kernel/asm-offsets_64.c \
            include/acpi \
            include/asm-generic/current.h \
            include/asm-generic/fixmap.h \
            include/asm-generic/memory_model.h \
            include/asm-generic/pgtable_uffd.h \
            include/asm-generic/pgtable-nopmd.h \
            include/asm-generic/pgtable-nopud.h \
            include/asm-generic/qrwlock.h \
            include/asm-generic/qspinlock.h \
            include/linux/arm-smccc.h \
            include/linux/arm_sdei.h \
            include/linux/cper.h \
            include/linux/crypto.h \
            include/linux/efi.h \
            include/linux/page_table_check.h \
            include/linux/pgtable.h \
            include/linux/pstore.h \
            include/uapi/asm-generic/ucontext.h \
            include/uapi/linux/arm_sdei.h \
            kernel/bounds.c \
            kernel/time/timeconst.bc

# needed for gen_crc32table
LX_FILES += lib/gen_crc32table.c \
            lib/crc32.c

content: src/linux/include/linux/kvm_host.h
src/linux/include/linux/kvm_host.h: # cut dependencies from kvm via dummy header
	mkdir -p $(dir $@)
	touch $@

# add content listed in the repository's source.list or dep.list files
LX_FILE_LISTS := $(shell find -H $(REP_DIR) -name dep.list -or -name source.list)
LX_FILES += $(shell cat $(LX_FILE_LISTS))
LX_FILES := $(sort $(LX_FILES))
MIRRORED_FROM_PORT_DIR += $(addprefix $(LX_REL_DIR)/,$(LX_FILES))

content: $(MIRRORED_FROM_PORT_DIR)
$(MIRRORED_FROM_PORT_DIR):
	mkdir -p $(dir $@)
	cp -r $(addprefix $(PORT_DIR)/,$@) $@

content: LICENSE
LICENSE:
	cp $(PORT_DIR)/src/linux/COPYING $@
