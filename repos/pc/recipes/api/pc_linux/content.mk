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

LX_FILES += $(shell cd $(LX_ABS_DIR); find -name "Kconfig*" -printf "%P\n")

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
