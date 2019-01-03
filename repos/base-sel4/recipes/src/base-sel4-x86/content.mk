include $(GENODE_DIR)/repos/base/recipes/src/base_content.inc

content: README
README:
	cp $(REP_DIR)/recipes/src/base-sel4-x86/README $@

content: lib/import etc include/sel4
lib/import etc include/sel4:
	$(mirror_from_rep_dir)

content: src/kernel/sel4
src/kernel:
	$(mirror_from_rep_dir)

KERNEL_PORT_DIR := $(call port_dir,$(REP_DIR)/ports/sel4)

src/kernel/sel4: src/kernel
	cp -r $(KERNEL_PORT_DIR)/src/kernel/sel4/* $@

content:
	for spec in x86_32 x86_64 arm; do \
	  mv lib/mk/spec/$$spec/ld-sel4.mk lib/mk/spec/$$spec/ld.mk; \
	  done;
	sed -i "s/ld-sel4/ld/"          src/lib/ld/sel4/target.mk
	sed -i "s/pit_timer_drv/timer/" src/timer/pit/target.inc

