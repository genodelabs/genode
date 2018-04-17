MIRROR_FROM_REP_DIR := lib/mk/egl.mk \
                       lib/mk/egl_i965.mk \
                       lib/mk/egl_swrast.mk \
                       lib/mk/glapi.mk \
                       lib/mk/i965.mk \
                       lib/mk/mesa.inc \
                       lib/mk/mesa_api.mk \
                       lib/mk/mesa-common.inc \
                       lib/mk/spec/arm/mesa.mk \
                       lib/mk/spec/x86_32/mesa.mk \
                       lib/mk/spec/x86_64/mesa.mk \
                       lib/mk/swrast.mk \
                       src/lib/mesa

content: $(MIRROR_FROM_REP_DIR) src/lib/mesa/target.mk

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

src/lib/mesa/target.mk:
	mkdir -p $(dir $@)
	echo "LIBS = mesa" > $@

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/mesa)

MIRROR_FROM_PORT_DIR := src/lib/mesa/src \

content: $(MIRROR_FROM_PORT_DIR)

$(MIRROR_FROM_PORT_DIR):
	mkdir -p $(dir $@)
	cp -r $(PORT_DIR)/$@ $(dir $@)

content: LICENSE

LICENSE:
	cp $(PORT_DIR)/src/lib/mesa/docs/COPYING $@
