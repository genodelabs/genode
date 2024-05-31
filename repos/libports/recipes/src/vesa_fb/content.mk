SRC_DIR := src/driver/framebuffer/vesa
include $(GENODE_DIR)/repos/base/recipes/src/content.inc

content: src/lib/x86emu include/x86emu lib/mk/x86emu.mk

src/lib/x86emu:
	mkdir -p $(dir $@)
	cp -r $(X86_EMU_PORT_DIR)/$@ $@

include/x86emu:
	mkdir -p $(dir $@)
	cp -r $(X86_EMU_PORT_DIR)/$@ $@
	cp -r $(REP_DIR)/$@/* $@

lib/mk/x86emu.mk:
	$(mirror_from_rep_dir)

X86_EMU_PORT_DIR := $(call port_dir,$(REP_DIR)/ports/x86emu)
