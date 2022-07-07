MIRROR_FROM_REP_DIR := lib/mk/libdrm.mk \
                       lib/mk/libdrm.inc \
                       lib/mk/spec/arm_v8/libdrm.mk \
                       lib/mk/spec/x86_64/libdrm.mk \
                       include/libdrm/ioctl_dispatch.h \
                       src/lib/libdrm/include \
                       src/lib/libdrm/dummies.c \
                       src/lib/libdrm/ioctl_dummy.cc \
                       src/lib/libdrm/ioctl_iris.cc \
                       src/lib/libdrm/ioctl_etnaviv.cc \
                       src/lib/libdrm/ioctl_dispatch.cc \
                       src/lib/libdrm/ioctl_lima.cc

content: $(MIRROR_FROM_REP_DIR) src/lib/libdrm/target.mk

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

src/lib/libdrm/target.mk:
	mkdir -p $(dir $@)
	echo "LIBS = libdrm" > $@

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/libdrm)

MIRROR_FROM_PORT_DIR := $(addprefix src/lib/libdrm/,\
                        xf86drm.c \
                        xf86drmHash.c \
                        xf86drmMode.c \
                        xf86drmRandom.c \
                        xf86drmSL.c)

content: $(MIRROR_FROM_PORT_DIR)

$(MIRROR_FROM_PORT_DIR):
	mkdir -p $(dir $@)
	cp -r $(PORT_DIR)/$@ $(dir $@)

content: LICENSE

LICENSE:
	echo "MIT, see header files" > $@

