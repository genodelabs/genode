include $(GENODE_DIR)/repos/base/recipes/src/base_content.inc

FROM_BASE_FOC := include/foc include/foc_native_cpu

content: $(FROM_BASE_FOC)

$(FROM_BASE_FOC):
	$(mirror_from_rep_dir)

content: README
README:
	cp $(RECIPE_DIR)/README $@

content: lib/import config etc
lib/import config etc:
	$(mirror_from_rep_dir)

content: src/kernel/foc
src/kernel:
	$(mirror_from_rep_dir)

KERNEL_PORT_DIR := $(call port_dir,$(REP_DIR)/ports/foc)

src/kernel/foc: src/kernel
	cp -r $(KERNEL_PORT_DIR)/src/kernel/foc/* $@

content:
	for spec in x86_32 x86_64 arm; do \
	  mv lib/mk/spec/$$spec/ld-foc.mk lib/mk/spec/$$spec/ld.mk; \
	  done;
	sed -i "s/ld-foc/ld/"           src/lib/ld/foc/target.mk
	sed -i "s/foc_timer_drv/timer/" src/timer/foc/target.mk

