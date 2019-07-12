include $(GENODE_DIR)/repos/base/recipes/src/base_content.inc

content: src/bootstrap

src/bootstrap:
	$(mirror_from_rep_dir)

content: generalize_target_names remove_other_board_libs

generalize_target_names: lib/mk src/lib src/timer
	for spec in arm riscv x86_64; do \
	  mv lib/mk/spec/$$spec/ld-hw.mk lib/mk/spec/$$spec/ld.mk; \
	  done;
	sed -i "s/ld-hw/ld/"           src/lib/ld/hw/target.mk
	sed -i "s/hw_timer_drv/timer/" src/timer/hw/target.mk

remove_other_board_libs: lib/mk
	find lib/mk/spec -name core-hw-*.mk -o -name bootstrap-hw-*.mk |\
		grep -v "hw-$(BOARD).mk" | xargs rm -rf
