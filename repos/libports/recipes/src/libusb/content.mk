MIRROR_FROM_REP_DIR := lib/mk/libusb.mk lib/import/import-libusb.mk \
                       include/libc-plugin/plugin.h \
                       src/lib/libc/internal/thread_create.h \
                       src/lib/libc/internal/types.h \

content: src/lib/libusb LICENSE $(MIRROR_FROM_REP_DIR)

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/libusb)

src/lib/libusb:
	mkdir -p $(dir $@)
	cp -r $(PORT_DIR)/src/lib/libusb $@
	echo "LIBS = libusb" > $@/target.mk
	$(mirror_from_rep_dir)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

LICENSE:
	cp $(PORT_DIR)/src/lib/libusb/COPYING $@
