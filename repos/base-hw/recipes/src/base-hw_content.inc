include $(GENODE_DIR)/repos/base/recipes/src/base_content.inc

content: src/bootstrap

src/bootstrap:
	$(mirror_from_rep_dir)


TIMER_SRC := main.cc target.inc hw include

content: src/drivers/timer
src/drivers/timer:
	mkdir -p $@
	cp -r $(addprefix $(GENODE_DIR)/repos/os/$@/,$(TIMER_SRC)) $@

content: include/spec/x86_32/trace/timestamp.h include/spec/x86_64/trace/timestamp.h

include/spec/%/trace/timestamp.h:
	mkdir -p $(dir $@)
	cp $(GENODE_DIR)/repos/os/$@ $@


content: generalize_target_names

generalize_target_names: lib/mk src/lib src/drivers/timer
	for spec in arm riscv x86_64; do \
	  mv lib/mk/spec/$$spec/ld-hw.mk lib/mk/spec/$$spec/ld.mk; \
	  done;
	sed -i "s/ld-hw/ld/"           src/lib/ld/hw/target.mk
	sed -i "s/hw_timer_drv/timer/" src/drivers/timer/hw/target.mk

