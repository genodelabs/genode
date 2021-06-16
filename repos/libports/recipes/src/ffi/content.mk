MIRROR_FROM_REP_DIR := lib/mk/spec/arm/ffi.mk \
                       lib/mk/spec/arm_64/ffi.mk \
                       lib/mk/spec/x86_64/ffi.mk
content: src/lib/ffi $(MIRROR_FROM_REP_DIR) LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/ffi)

src/lib/ffi:
	mkdir -p $@
	cp -r $(PORT_DIR)/src/lib/ffi/* $@
	cp -r $(REP_DIR)/src/lib/ffi/* $@
	echo "LIBS = ffi" > $@/target.mk

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

LICENSE:
	cp $(PORT_DIR)/src/lib/ffi/LICENSE $@
