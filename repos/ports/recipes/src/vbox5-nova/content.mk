LIB_MK_FILES := $(notdir $(wildcard $(REP_DIR)/lib/mk/virtualbox5-*)) \
                spec/nova/virtualbox5-nova.mk

MIRROR_FROM_REP_DIR := src/virtualbox5 \
                       src/virtualbox/include \
                       src/virtualbox/network.cpp \
                       src/virtualbox/vmm.h \
                       src/virtualbox/sup.h \
                       src/virtualbox/mm.h \
                       src/virtualbox/util.h \
                       src/virtualbox/dynlib.cc \
                       src/virtualbox/libc.cc \
                       src/virtualbox/logger.cc \
                       src/virtualbox/pdm.cc \
                       src/virtualbox/rt.cc \
                       src/virtualbox/thread.cc \
                       include/vmm \
                       $(addprefix lib/mk/,$(LIB_MK_FILES))

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

# omit virtualbox5-rem binary (12 MiB) from binary archive
content: disable_virtualbox_rem

disable_virtualbox_rem: $(MIRROR_FROM_REP_DIR)
	rm src/virtualbox5/target.mk

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/virtualbox5)

MIRROR_FROM_PORT_DIR := src/app/virtualbox src/app/virtualbox_sdk \
                        VBoxAPIWrap VirtualBox_stripped.xidl

content: $(MIRROR_FROM_PORT_DIR)

$(MIRROR_FROM_PORT_DIR):
	mkdir -p $(dir $@)
	cp -r $(PORT_DIR)/$@ $(dir $@)

MIRROR_FROM_LIBPORTS := lib/mk/libc_pipe.mk \
                        src/lib/libc_pipe \
                        lib/mk/libc_terminal.mk \
                        src/lib/libc_terminal \
                        lib/mk/libc-mem.mk \
                        lib/mk/libc-common.inc \
                        src/lib/libc/libc_mem_alloc.cc \
                        src/lib/libc/libc_mem_alloc.h \
                        src/lib/libc/libc_init.h \
                        src/lib/libc/libc_errno.h \
                        include/libc-plugin \
                        lib/import/import-qemu-usb_include.mk \
                        lib/mk/qemu-usb_include.mk \
                        lib/mk/qemu-usb.mk \
                        include/qemu \
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

MIRROR_FROM_OS := src/drivers/input/spec/ps2/scan_code_set_1.h \
                  include/pointer/shape_report.h \

content: $(MIRROR_FROM_OS)

$(MIRROR_FROM_OS):
	mkdir -p $(dir $@)
	cp -r $(GENODE_DIR)/repos/os/$@ $(dir $@)

content: LICENSE

LICENSE:
	echo "GNU GPL version 2, see src/app/virtualbox/COPYING" > $@

