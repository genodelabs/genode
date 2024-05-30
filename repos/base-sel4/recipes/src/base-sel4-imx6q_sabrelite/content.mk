include $(GENODE_DIR)/repos/base/recipes/src/base_content.inc

content: include/os/attached_mmio.h

include/%.h:
	mkdir -p $(dir $@)
	cp $(GENODE_DIR)/repos/os/$@ $@

content: README
README:
	cp $(REP_DIR)/recipes/src/base-sel4-imx6q_sabrelite/README $@

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

content: etc/board.conf

etc/board.conf:
	echo "BOARD = imx6q_sabrelite" > etc/board.conf

content:
	mv lib/mk/spec/arm/ld-sel4.mk lib/mk/spec/arm/ld.mk;
	sed -i "s/imx6_timer/timer/" src/timer/epit/imx6/target.inc
	find lib/mk/spec -name kernel-sel4-*.mk -o -name syscall-sel4-*.mk |\
		grep -v "sel4-imx6q_sabrelite.mk" | xargs rm -rf

