MIRROR_FROM_REP_DIR := lib/mk/drm.mk \
                       src/lib/drm/include \
                       src/lib/drm/dummies.cc \
                       src/lib/drm/ioctl.cc

content: $(MIRROR_FROM_REP_DIR) src/lib/drm/target.mk

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

src/lib/drm/target.mk:
	mkdir -p $(dir $@)
	echo "LIBS = drm" > $@

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/drm)

MIRROR_FROM_PORT_DIR := $(addprefix src/lib/drm/,\
                        intel/intel_bufmgr.c \
                        intel/intel_bufmgr_gem.c \
                        intel/intel_decode.c \
                        xf86drm.c \
                        xf86drmHash.c \
                        xf86drmRandom.c)

content: $(MIRROR_FROM_PORT_DIR)

$(MIRROR_FROM_PORT_DIR):
	mkdir -p $(dir $@)
	cp -r $(PORT_DIR)/$@ $(dir $@)

content: LICENSE

LICENSE:
	echo "MIT, see header files" > $@
