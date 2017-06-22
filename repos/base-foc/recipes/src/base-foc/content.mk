include $(GENODE_DIR)/repos/base/recipes/src/base_content.inc

FROM_BASE_FOC := include/foc include/foc_native_cpu

content: $(FROM_BASE_FOC)

$(FROM_BASE_FOC):
	$(mirror_from_rep_dir)


TIMER_SRC := main.cc target.inc foc include periodic fiasco

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
	cp $(REP_DIR)/recipes/src/base-foc/README $@

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
	sed -i "s/ld-foc/ld/"     src/lib/ld/foc/target.mk
	sed -i "s/foc_timer_drv/timer/" src/drivers/timer/foc/target.mk

