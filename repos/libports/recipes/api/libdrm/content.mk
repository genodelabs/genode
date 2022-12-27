MIRROR_FROM_REP_DIR := lib/import/import-libdrm.mk \
                       lib/symbols/libdrm

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/libdrm)

content: include

include:
	mkdir -p $@
	cp -r $(PORT_DIR)/src/lib/libdrm/*.h $@
	mkdir -p $@/drm
	cp -r $(PORT_DIR)/src/lib/libdrm/include/drm/*.h $@/drm
	mkdir -p $@/intel
	cp -r $(PORT_DIR)/src/lib/libdrm/intel/*.h $@/intel
	mkdir -p $@/etnaviv
	cp -r $(PORT_DIR)/src/lib/libdrm/etnaviv/*.h $@/etnaviv
	mkdir -p $@/libdrm
	cp $(REP_DIR)/include/libdrm/ioctl_dispatch.h $@/libdrm

content: LICENSE

LICENSE:
	echo "MIT, see header files" > $@

