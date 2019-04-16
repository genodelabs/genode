include $(GENODE_DIR)/repos/base/recipes/src/base_content.inc

content: enable_board_spec
enable_board_spec: etc/specs.conf
	echo "SPECS += imx7d_sabre" >> etc/specs.conf

content: include/os/attached_mmio.h

include/%.h:
	mkdir -p $(dir $@)
	cp $(GENODE_DIR)/repos/os/$@ $@

content: README
README:
	cp $(REP_DIR)/recipes/src/base-sel4-imx7d_sabre/README $@

content: lib/import etc include/sel4
lib/import etc include/sel4:
	$(mirror_from_rep_dir)

content: src/tool/sel4_tools
src/kernel:
	$(mirror_from_rep_dir)

KERNEL_PORT_DIR := $(call port_dir,$(REP_DIR)/ports/sel4)

src/kernel/sel4: src/kernel
	cp -r $(KERNEL_PORT_DIR)/src/kernel/sel4/* $@

ELFLOADER_PORT_DIR := $(call port_dir,$(REP_DIR)/ports/sel4_tools)
src/tool/sel4_tools: src/kernel/sel4
	mkdir -p $@
	cp -r $(ELFLOADER_PORT_DIR)/src/tool/sel4_tools/* $@

content:
	mv lib/mk/spec/arm/ld-sel4.mk lib/mk/spec/arm/ld.mk;
	sed -i "s/ld-sel4/ld/"          src/lib/ld/sel4/target.mk
	sed -i "s/imx7_timer_drv/timer/" src/timer/gpt/imx7/target.inc

