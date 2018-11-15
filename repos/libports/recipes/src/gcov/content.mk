content: include \
         lib/mk/gcov-libcommon.mk \
         lib/mk/gcov-libcpp.mk \
         lib/mk/gcov-libiberty.mk \
         src/gcov \
         src/app/gcov \
         LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/gcov)

include:
	mkdir $@
	cp -r $(PORT_DIR)/include/* $@/

lib/mk/%.mk:
	$(mirror_from_rep_dir)

src/lib/%:
	$(mirror_from_rep_dir)

src/gcov:
	mkdir -p $(dir $@)
	cp -r $(PORT_DIR)/src/gcov $@

src/app/gcov:
	$(mirror_from_rep_dir)

LICENSE:
	cp $(PORT_DIR)/src/gcov/COPYING $@
