SRC_DIR = src/test/libc_pipe
include $(GENODE_DIR)/repos/base/recipes/src/content.inc

MIRROR_FROM_REP_DIR := include/libc-plugin \
                       lib/mk/libc_pipe.mk \
                       src/lib/libc_pipe

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)
