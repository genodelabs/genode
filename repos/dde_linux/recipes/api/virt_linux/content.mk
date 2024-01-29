#
# Content hosted in the dde_linux repository
#

MIRRORED_FROM_REP_DIR := lib/import/import-lx_emul_common.inc \
                         lib/import/import-virt_lx_emul.mk \
                         lib/mk/spec/arm_v6/virt_linux_generated.mk \
                         lib/mk/spec/arm_v7/virt_linux_generated.mk \
                         lib/mk/spec/arm_v8/virt_linux_generated.mk \
                         lib/mk/spec/x86_32/virt_linux_generated.mk \
                         lib/mk/spec/x86_64/virt_linux_generated.mk \
                         lib/mk/virt_linux_generated.inc \
                         lib/mk/virt_lx_emul.mk \
                         src/include/lx_emul \
                         src/include/lx_kit \
                         src/include/spec/arm_v6/lx_kit \
                         src/include/spec/arm_v7/lx_kit \
                         src/include/spec/arm_v8/lx_kit \
                         src/include/spec/x86_32/lx_kit \
                         src/include/spec/x86_64/lx_kit \
                         src/include/virt_linux \
                         src/lib/lx_emul \
                         src/lib/lx_kit \
                         src/virt_linux/target.inc \
                         src/virt_linux/arm_v6/target.inc \
                         src/virt_linux/arm_v7/target.inc \
                         src/include/lx_user

content: $(MIRRORED_FROM_REP_DIR)

$(MIRRORED_FROM_REP_DIR):
	$(mirror_from_rep_dir)


#
# Content from the Linux source tree
#

PORT_DIR   := $(call port_dir,$(GENODE_DIR)/repos/dde_linux/ports/linux)
LX_REL_DIR := src/linux
LX_ABS_DIR := $(addsuffix /$(LX_REL_DIR),$(PORT_DIR))

LX_FILES += $(shell cd $(LX_ABS_DIR); find -name "Kconfig*" -printf "%P\n")

# add content listed in the repository's source.list or dep.list files
LX_FILE_LISTS := $(shell find -H $(REP_DIR) -name dep.list -or -name source.list)
LX_FILES      += $(shell cat $(LX_FILE_LISTS))
LX_FILES      := $(sort $(LX_FILES))
MIRRORED_FROM_PORT_DIR += $(addprefix $(LX_REL_DIR)/,$(LX_FILES))

content: $(MIRRORED_FROM_PORT_DIR)
$(MIRRORED_FROM_PORT_DIR):
	mkdir -p $(dir $@)
	cp -r $(addprefix $(PORT_DIR)/,$@) $@

content: LICENSE
LICENSE:
	cp $(PORT_DIR)/src/linux/COPYING $@
