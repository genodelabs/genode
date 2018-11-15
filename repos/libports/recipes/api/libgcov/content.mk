content: include \
         lib/mk/libgcov.mk \
         src/gcov \
         src/lib/gcov \
         LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/gcov)

include:
	mkdir $@
	cp -r $(PORT_DIR)/include/* $@/
	cp -r $(GENODE_DIR)/repos/os/include/file_system $@/

lib/mk/libgcov.mk:
	$(mirror_from_rep_dir)

src/gcov:
	mkdir -p $@
	cp -r $(PORT_DIR)/src/gcov/include $@/
	cp -r $(PORT_DIR)/src/gcov/gcc $@/
	cp -r $(PORT_DIR)/src/gcov/libgcc $@/

src/lib/gcov:
	$(mirror_from_rep_dir)
	echo "LIBS = libgcov" > $@/target.mk

LICENSE:
	cp $(PORT_DIR)/src/gcov/COPYING $@
