include $(GENODE_DIR)/repos/base/recipes/src/base_content.inc

content: README
README:
	cp $(REP_DIR)/recipes/src/base-fiasco/README $@

content: lib/import config
lib/import config:
	$(mirror_from_rep_dir)

content: src/kernel/fiasco
src/kernel:
	$(mirror_from_rep_dir)

KERNEL_PORT_DIR := $(call port_dir,$(REP_DIR)/ports/fiasco)

src/kernel/fiasco: src/kernel
	cp -r $(KERNEL_PORT_DIR)/src/kernel/fiasco/* $@

content:
	for spec in x86_32; do \
	  mv lib/mk/spec/$$spec/ld-fiasco.mk lib/mk/spec/$$spec/ld.mk; \
	  done;
	sed -i "s/fiasco_timer/timer/" src/timer/fiasco/target.mk

