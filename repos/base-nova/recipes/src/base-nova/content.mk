include $(GENODE_DIR)/repos/base/recipes/src/base_content.inc


TIMER_SRC := main.cc target.inc nova include

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
	cp $(REP_DIR)/recipes/src/base-nova/README $@


content: src/kernel/nova
src/kernel:
	$(mirror_from_rep_dir)

KERNEL_PORT_DIR := $(call port_dir,$(REP_DIR)/ports/nova)

src/kernel/nova: src/kernel
	cp -r $(KERNEL_PORT_DIR)/src/kernel/nova/* $@


content:
	for spec in x86_32 x86_64; do \
	  mv lib/mk/spec/$$spec/ld-nova.mk   lib/mk/spec/$$spec/ld.mk; \
	  done;
	sed -i "s/ld-nova/ld/"           src/lib/ld/nova/target.mk
	sed -i "s/nova_timer_drv/timer/" src/drivers/timer/nova/target.mk

