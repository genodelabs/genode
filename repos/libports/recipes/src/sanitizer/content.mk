MIRROR_FROM_REP_DIR := lib/mk/libsanitizer_common.mk \
                       lib/mk/libubsan.mk

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

content: src/lib/sanitizer

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/sanitizer)

src/lib/sanitizer:
	mkdir -p $@
	cp -r $(PORT_DIR)/src/lib/sanitizer/* $@
	echo "LIBS = libsanitizer_common libubsan" > $@/target.mk

content: LICENSE

LICENSE:
	cp $(PORT_DIR)/src/lib/sanitizer/LICENSE.TXT $@

