include $(GENODE_DIR)/repos/base/recipes/src/base_content.inc


TIMER_SRC := main.cc target.inc pit include

content: src/drivers/timer
src/drivers/timer:
	mkdir -p $@
	cp -r $(addprefix $(GENODE_DIR)/repos/os/$@/,$(TIMER_SRC)) $@

content: include/spec/x86_32/trace/timestamp.h include/spec/x86_64/trace/timestamp.h

include/spec/%/trace/timestamp.h:
	mkdir -p $(dir $@)
	cp $(GENODE_DIR)/repos/os/$@ $@


content: README
README:
	cp $(REP_DIR)/recipes/src/base-pistachio/README $@

content: lib/import config etc
lib/import config etc:
	$(mirror_from_rep_dir)

content: src/kernel/pistachio
src/kernel:
	$(mirror_from_rep_dir)

KERNEL_PORT_DIR := $(call port_dir,$(REP_DIR)/ports/pistachio)

src/kernel/pistachio: src/kernel
	cp -r $(KERNEL_PORT_DIR)/src/kernel/pistachio/* $@


content:
	for spec in x86_32; do \
	  mv lib/mk/spec/$$spec/ld-pistachio.mk lib/mk/spec/$$spec/ld.mk; \
	  done;
	sed -i "s/ld-pistachio/ld/"     src/lib/ld/pistachio/target.mk
	sed -i "s/pit_timer_drv/timer/" src/drivers/timer/pit/target.mk

