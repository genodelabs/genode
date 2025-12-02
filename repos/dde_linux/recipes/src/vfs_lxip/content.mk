LIB_MK := $(addprefix lib/mk/,lxip.inc vfs_lxip.mk) \
          $(foreach SPEC,x86_32 x86_64 arm_v6 arm_v7 arm_v8,lib/mk/spec/$(SPEC)/lxip.mk)

MIRROR_FROM_REP_DIR := $(LIB_MK) \
                       lib/import/import-lxip.mk \
                       src/lib/lxip \


MIRROR_FROM_OS := src/lib/vfs/ip/vfs.cc \
                  src/lib/vfs/ip/vfs_lxip.cc \
                  src/lib/vfs/ip/vfs_ip.h \
                  src/lib/vfs/ip/sockopt.h \
                  src/lib/vfs/ip/socket_error.h \
                  src/lib/vfs/ip/symbol.map

content: $(MIRROR_FROM_REP_DIR) $(MIRROR_FROM_OS)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

$(MIRROR_FROM_OS):
	mkdir -p $(dir $@)
	cp -r $(GENODE_DIR)/repos/os/$@ $@

PORT_DIR := $(call port_dir,$(GENODE_DIR)/repos/dde_linux/ports/linux)

content: LICENSE
LICENSE:
	cp $(PORT_DIR)/src/linux/COPYING $@
