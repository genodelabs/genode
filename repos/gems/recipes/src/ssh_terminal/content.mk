SRC_DIR := src/server/ssh_terminal
include $(GENODE_DIR)/repos/base/recipes/src/content.inc

MIRROR_FROM_LIBPORTS := lib/mk/libc_pipe.mk \
                        src/lib/libc_pipe \
                        include/libc-plugin

content: $(MIRROR_FROM_LIBPORTS)

$(MIRROR_FROM_LIBPORTS):
	mkdir -p $(dir $@)
	cp -r $(GENODE_DIR)/repos/libports/$@ $(dir $@)
