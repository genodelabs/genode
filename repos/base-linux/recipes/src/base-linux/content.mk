include $(GENODE_DIR)/repos/base/recipes/src/base_content.inc

content: lib/import src/ld

lib/import src/ld:
	$(mirror_from_rep_dir)

content: src/drivers/timer

TIMER_SRC := main.cc target.inc linux include periodic

src/drivers/timer:
	mkdir -p $@
	cp -r $(addprefix $(GENODE_DIR)/repos/os/$@/,$(TIMER_SRC)) $@

content:
	for spec in x86_32 x86_64 arm; do \
	  mv lib/mk/spec/$$spec/ld-linux.mk lib/mk/spec/$$spec/ld.mk; done;
	sed -i "s/core-linux/core/"       src/core/linux/target.mk
	sed -i "s/ld-linux/ld/"           src/lib/ld/linux/target.mk
	sed -i "s/linux_timer_drv/timer/" src/drivers/timer/linux/target.mk

