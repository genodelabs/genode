LIB_MK_FILES      := $(notdir $(wildcard $(REP_DIR)/lib/mk/virtualbox6*))
LIB_MK_ARCH_FILES := $(notdir $(wildcard $(REP_DIR)/lib/mk/spec/x86_64/virtualbox6*))

MIRROR_FROM_REP_DIR := $(addprefix lib/mk/,$(LIB_MK_FILES)) \
                       $(addprefix lib/mk/spec/x86_64/,$(LIB_MK_ARCH_FILES))

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

content: disable_assertions

disable_assertions: $(MIRROR_FROM_REP_DIR)
	rm lib/mk/virtualbox6-debug.inc
	touch lib/mk/virtualbox6-debug.inc

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/virtualbox6)

content: src/virtualbox6 src/virtualbox6_sdk

src/virtualbox6:
	mkdir -p $(dir $@)
	cp -r $(REP_DIR)/$@ $(dir $@)
	cp -r $(PORT_DIR)/$@ $(dir $@)

src/virtualbox6_sdk:
	mkdir -p $(dir $@)
	cp -r $(PORT_DIR)/$@ $(dir $@)

MIRROR_FROM_LIBPORTS := \
	include/qemu \
	lib/import/import-qemu-usb_include.mk \
	lib/mk/libc-common.inc \
	lib/mk/libc-mem.mk \
	lib/mk/qemu-usb.inc \
	lib/mk/qemu-usb_include.mk \
	lib/mk/qemu-usb-webcam.inc \
	lib/mk/spec/x86_32/qemu-usb.mk \
	lib/mk/spec/x86_64/qemu-usb.mk \
	src/lib/libc/internal/init.h \
	src/lib/libc/internal/mem_alloc.h \
	src/lib/libc/internal/monitor.h \
	src/lib/libc/internal/pthread.h \
	src/lib/libc/internal/thread_create.h \
	src/lib/libc/internal/timer.h \
	src/lib/libc/internal/types.h \
	src/lib/libc/libc_mem_alloc.cc \
	src/lib/libc/spec/x86_64/internal/call_func.h \
	src/lib/qemu-usb

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

