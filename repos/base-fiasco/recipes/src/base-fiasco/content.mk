include $(GENODE_DIR)/repos/base/recipes/src/base_content.inc


TIMER_SRC := main.cc target.inc include periodic fiasco

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
	cp $(REP_DIR)/recipes/src/base-fiasco/README $@

content: lib/import config etc
lib/import config etc:
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
	sed -i "s/ld-fiasco/ld/"     src/lib/ld/fiasco/target.mk
	sed -i "s/fiasco_timer_drv/timer/" src/drivers/timer/fiasco/target.mk

