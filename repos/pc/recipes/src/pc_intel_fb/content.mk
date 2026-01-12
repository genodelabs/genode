MIRROR_FROM_REP_DIR := src/driver/framebuffer/intel/pc \
                       src/lib/pc/lx_emul \
                       src/include

content: $(MIRROR_FROM_REP_DIR)

PORT_DIR := $(call port_dir,$(GENODE_DIR)/repos/dde_linux/ports/legacy_linux)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

content: LICENSE
LICENSE:
	cp $(PORT_DIR)/src/linux/COPYING $@
