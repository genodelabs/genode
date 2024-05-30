include $(GENODE_DIR)/repos/base/recipes/src/base_content.inc

RECIPE_DIR := $(REP_DIR)/recipes/src/base-foc-$(BOARD)
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
	tar -C $(KERNEL_PORT_DIR)/src/kernel/foc --exclude=.git -cf - . |\
		tar -C $@ -xf -

content: etc/board.conf remove_other_board_libs

etc/board.conf:
	echo "BOARD = $(BOARD)" > etc/board.conf

remove_other_board_libs: lib/mk
	find lib/mk/spec -name kernel-foc-*.mk -o -name syscall-foc-*.mk |\
		grep -v "foc-$(BOARD).mk" | xargs rm -rf

content:
	for spec in x86_32 x86_64 arm arm_64; do \
	  mv lib/mk/spec/$$spec/ld-foc.mk lib/mk/spec/$$spec/ld.mk; \
	  done;
	sed -i "s/foc_timer/timer/" src/timer/foc/target.mk
