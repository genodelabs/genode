MIRROR_FROM_REP_DIR := src/driver/nic/pc \
                       src/lib/pc/lx_emul \
                       src/include

content: $(MIRROR_FROM_REP_DIR)
$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

#MIRROR_FROM_OS_DIR  := src/lib/genode_c_api/uplink.cc
#
#content: $((MIRROR_FROM_OS_DIR)
#$(MIRROR_FROM_OS_DIR):
#	mkdir -p $(dir $@)
#	cp -r $(GENODE_DIR)/repos/os/$@ $@

PORT_DIR := $(call port_dir,$(GENODE_DIR)/repos/dde_linux/ports/linux)

content: LICENSE
LICENSE:
	cp $(PORT_DIR)/src/linux/COPYING $@
