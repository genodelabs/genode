MIRRORED_FROM_REP_DIR := \
	lib/import/import-lx_emul_common.inc \
	lib/import/import-virt_lx_emul.mk \
	lib/mk/spec/arm_64/virt_linux_generated.mk \
	lib/mk/spec/arm_64/wireguard.mk \
	lib/mk/spec/arm_64/wireguard_lx_inc_dirs.mk \
	lib/mk/spec/x86_64/virt_linux_generated.mk \
	lib/mk/spec/x86_64/wireguard.mk \
	lib/mk/spec/x86_64/wireguard_lx_inc_dirs.mk \
	lib/mk/virt_linux_generated.inc \
	lib/mk/virt_lx_emul.mk \
	lib/mk/wireguard.inc \
	lib/mk/wireguard_lx_inc_dirs.inc \
	src/app/wireguard \
	src/include/lx_emul \
	src/include/lx_kit \
	src/include/spec/arm_64/lx_kit \
	src/include/spec/x86_64/lx_kit \
	src/include/virt_linux \
	src/lib/lx_emul \
	src/lib/lx_kit \
	src/virt_linux/target.inc \
	src/include/lx_user

content: $(MIRRORED_FROM_REP_DIR)

$(MIRRORED_FROM_REP_DIR):
	$(mirror_from_rep_dir)


#
# FIXME
#
# The following code was copied from the recipe
# repos/pc/recipes/api/pc_linux/content.mk. It should be replaced by
# creating a new api/virt_linux recipe and adding it to wireguard/used_apis.
#

#
# Content from the Linux source tree
#

PORT_DIR := $(call port_dir,$(GENODE_DIR)/repos/dde_linux/ports/linux)
LX_REL_DIR := src/linux
LX_ABS_DIR := $(addsuffix /$(LX_REL_DIR),$(PORT_DIR))

# ingredients needed for creating a Linux build directory / generated headers
LX_FILES += Kbuild \
            Makefile \
            arch/arm64/Makefile \
            arch/arm64/boot/dts \
            arch/arm64/configs \
            arch/arm64/crypto/poly1305-armv8.pl \
            arch/arm64/include \
            arch/arm64/kernel/asm-offsets.c \
            arch/arm64/kernel/vdso/Makefile \
            arch/arm64/kernel/vdso/gen_vdso_offsets.sh \
            arch/arm64/kernel/vdso/note.S \
            arch/arm64/kernel/vdso/sigreturn.S \
            arch/arm64/kernel/vdso/vdso.lds.S \
            arch/arm64/kernel/vdso/vgettimeofday.c \
            arch/arm64/tools/Makefile \
            arch/arm64/tools/cpucaps \
            arch/arm64/tools/gen-cpucaps.awk \
            arch/x86/Makefile \
            arch/x86/Makefile_32.cpu \
            arch/x86/configs \
            arch/x86/entry/syscalls/Makefile \
            arch/x86/entry/syscalls/syscall_32.tbl \
            arch/x86/entry/syscalls/syscall_64.tbl \
            arch/x86/include \
            arch/x86/kernel/asm-offsets.c \
            arch/x86/kernel/asm-offsets_64.c \
            arch/x86/tools/Makefile \
            arch/x86/tools/relocs.c \
            arch/x86/tools/relocs.h \
            arch/x86/tools/relocs_32.c \
            arch/x86/tools/relocs_64.c \
            arch/x86/tools/relocs_common.c \
            include \
            kernel/bounds.c \
            kernel/configs/tiny-base.config \
            kernel/configs/tiny.config \
            kernel/time/timeconst.bc \
            lib/vdso/Makefile \
            lib/vdso/gettimeofday.c \
            scripts/Kbuild.include \
            scripts/Makefile \
            scripts/Makefile.asm-generic \
            scripts/Makefile.build \
            scripts/Makefile.compiler \
            scripts/Makefile.extrawarn \
            scripts/Makefile.host \
            scripts/Makefile.lib \
            scripts/as-version.sh \
            scripts/asn1_compiler.c \
            scripts/atomic/check-atomics.sh \
            scripts/basic/Makefile \
            scripts/basic/fixdep.c \
            scripts/cc-version.sh \
            scripts/checksyscalls.sh \
            scripts/config \
            scripts/dtc \
            scripts/extract-cert.c \
            scripts/gcc-goto.sh \
            scripts/kconfig/merge_config.sh \
            scripts/ld-version.sh \
            scripts/min-tool-version.sh \
            scripts/mod \
            scripts/remove-stale-files \
            scripts/setlocalversion \
            scripts/sorttable.c \
            scripts/sorttable.h \
            scripts/subarch.include \
            scripts/syscallhdr.sh \
            scripts/syscalltbl.sh \
            tools/include/tools \
            tools/objtool

LX_SCRIPTS_KCONFIG_FILES := $(notdir $(wildcard $(LX_ABS_DIR)/scripts/kconfig/*.c)) \
                            $(notdir $(wildcard $(LX_ABS_DIR)/scripts/kconfig/*.h)) \
                            Makefile lexer.l parser.y
LX_FILES += $(addprefix scripts/kconfig/,$(LX_SCRIPTS_KCONFIG_FILES)) \

LX_FILES += $(shell cd $(LX_ABS_DIR); find -name "Kconfig*" -printf "%P\n")

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
