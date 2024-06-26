LIB_MK := $(addprefix lib/mk/,lxip.inc vfs_lxip.mk) \
          $(foreach SPEC,x86_32 x86_64 arm_v6 arm_v7 arm_v8,lib/mk/spec/$(SPEC)/lxip.mk)

MIRROR_FROM_REP_DIR := $(LIB_MK) \
                       lib/import/import-lxip.mk \
                       src/lib/lxip \
                       src/lib/vfs/lxip


content: $(MIRROR_FROM_REP_DIR)
$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

PORT_DIR := $(call port_dir,$(GENODE_DIR)/repos/dde_linux/ports/legacy_linux)

content: LICENSE
LICENSE:
	cp $(PORT_DIR)/src/linux/COPYING $@
