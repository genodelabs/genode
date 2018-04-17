MIRROR_FROM_REP_DIR := lib/import/import-drm.mk \
                       lib/symbols/drm

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/drm)

content: include

include:
	mkdir -p $@
	cp -r $(PORT_DIR)/src/lib/drm/*.h $@
	cp -r $(PORT_DIR)/src/lib/drm/include/drm $@
	mkdir -p $@/intel
	cp -r $(PORT_DIR)/src/lib/drm/intel/*.h $@/intel

content: LICENSE

LICENSE:
	echo "MIT, see header files" > $@
