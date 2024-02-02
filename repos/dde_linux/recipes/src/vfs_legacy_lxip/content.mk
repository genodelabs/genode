LIB_MK := $(addprefix lib/mk/,legacy_lxip.mk lxip_include.mk vfs_legacy_lxip.mk)

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/dde_linux)

MIRROR_FROM_REP_DIR := $(LIB_MK) \
                       lib/import/import-lxip_include.mk \
                       include/lxip src/include/legacy src/lib/legacy/lx_kit \
                       src/lib/lx_kit/spec \
                       $(foreach SPEC, \
                                 arm_v6 arm_v7 arm_v8 x86 x86_32 x86_64, \
                                 src/include/spec/$(SPEC)) \
                       $(shell cd $(REP_DIR); find src/lib/legacy_lxip -type f) \
                       $(shell cd $(REP_DIR); find src/lib/vfs -type f)

MIRROR_FROM_PORT_DIR := $(shell cd $(PORT_DIR); find src/lib/lxip -type f)

content: $(MIRROR_FROM_REP_DIR) $(MIRROR_FROM_PORT_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

$(MIRROR_FROM_PORT_DIR):
	mkdir -p $(dir $@)
	cp -r $(PORT_DIR)/$@ $@

content: LICENSE
LICENSE:
	( echo "LxIP is based on Linux, which is licensed under the"; \
	  echo "GNU General Public License version 2, see:"; \
	  echo "https://www.kernel.org/pub/linux/kernel/COPYING" ) > $@
