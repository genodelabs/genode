include $(GENODE_DIR)/repos/base/recipes/src/base_content.inc

content: README
README:
	cp $(REP_DIR)/recipes/src/base-okl4/README $@

content: lib/import contrib
lib/import contrib:
	$(mirror_from_rep_dir)

content: src/kernel/okl4
src/kernel:
	$(mirror_from_rep_dir)

KERNEL_PORT_DIR := $(call port_dir,$(REP_DIR)/ports/okl4)

src/kernel/okl4: src/kernel
	cp -r $(KERNEL_PORT_DIR)/src/kernel/okl4/* $@

content:
	for spec in x86_32; do \
	  mv lib/mk/spec/$$spec/ld-okl4.mk lib/mk/spec/$$spec/ld.mk; \
	  done;
	sed -i "s/pit_timer/timer/" src/timer/pit/target.inc

