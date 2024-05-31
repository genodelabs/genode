LIB_MK_FILES := $(notdir $(wildcard $(REP_DIR)/lib/mk/virtualbox5-*)) \
                spec/nova/virtualbox5-nova.mk

MIRROR_FROM_REP_DIR := src/virtualbox5 \
                       src/virtualbox5/network.cpp \
                       src/virtualbox5/include \
                       include/vmm \
                       $(addprefix lib/mk/,$(LIB_MK_FILES))

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

# disable debug assertions
content: disable_assertions

disable_assertions: $(MIRROR_FROM_REP_DIR)
	rm lib/mk/virtualbox5-debug.inc
	touch lib/mk/virtualbox5-debug.inc

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/virtualbox5)

MIRROR_FROM_PORT_DIR := src/app/virtualbox src/app/virtualbox_sdk \
                        VBoxAPIWrap VirtualBox_stripped.xidl

content: $(MIRROR_FROM_PORT_DIR)

$(MIRROR_FROM_PORT_DIR):
	mkdir -p $(dir $@)
	cp -r $(PORT_DIR)/$@ $(dir $@)

MIRROR_FROM_LIBPORTS := lib/mk/libc-mem.mk \
                        lib/mk/libc-common.inc \
                        src/lib/libc/internal/init.h \
                        src/lib/libc/internal/mem_alloc.h \
                        src/lib/libc/internal/monitor.h \
                        src/lib/libc/internal/pthread.h \
                        src/lib/libc/internal/thread_create.h \
                        src/lib/libc/internal/timer.h \
                        src/lib/libc/internal/types.h \
                        src/lib/libc/libc_mem_alloc.cc \
                        lib/import/import-qemu-usb_include.mk \
                        lib/mk/qemu-usb_include.mk \
                        lib/mk/qemu-usb.inc \
                        lib/mk/qemu-usb-webcam.inc \
                        lib/mk/spec/x86_32/qemu-usb.mk \
                        lib/mk/spec/x86_64/qemu-usb.mk \
                        include/qemu \
                        src/lib/qemu-usb \

content: $(MIRROR_FROM_LIBPORTS)

$(MIRROR_FROM_LIBPORTS):
	mkdir -p $(dir $@)
	cp -r $(GENODE_DIR)/repos/libports/$@ $(dir $@)

QEMU_USB_PORT_DIR := $(call port_dir,$(GENODE_DIR)/repos/libports/ports/qemu-usb)

MIRROR_FROM_QEMU_USB_PORT_DIR := src/lib/qemu

content: $(MIRROR_FROM_QEMU_USB_PORT_DIR)

$(MIRROR_FROM_QEMU_USB_PORT_DIR):
	mkdir -p $(dir $@)
	cp -r $(QEMU_USB_PORT_DIR)/$@ $(dir $@)

MIRROR_FROM_OS := src/driver/ps2/scan_code_set_1.h \
                  include/pointer/shape_report.h \

content: $(MIRROR_FROM_OS)

$(MIRROR_FROM_OS):
	mkdir -p $(dir $@)
	cp -r $(GENODE_DIR)/repos/os/$@ $(dir $@)

content: LICENSE

LICENSE:
	echo "GNU GPL version 2, see src/app/virtualbox/COPYING" > $@

