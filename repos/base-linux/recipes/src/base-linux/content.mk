include $(GENODE_DIR)/repos/base/recipes/src/base_content.inc

content: lib/import src/ld

lib/import src/ld:
	$(mirror_from_rep_dir)

content:
	for spec in x86_32 x86_64 arm arm_64; do \
	  mv lib/mk/spec/$$spec/ld-linux.mk lib/mk/spec/$$spec/ld.mk; done;
	sed -i "/TARGET/s/core-linux/core/"       src/core/linux/target.mk
	sed -i "s/BOARD.*unknown/BOARD := linux/" lib/mk/core-linux.inc
	sed -i "s/linux_timer/timer/"             src/timer/linux/target.mk
	rm -rf src/initramfs

