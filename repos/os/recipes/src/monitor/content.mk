SRC_DIR = src/monitor
include $(GENODE_DIR)/repos/base/recipes/src/content.inc

MIRROR_FROM_REP_DIR += lib/mk/spec/arm_64/monitor_gdb_arch.mk \
                       lib/mk/spec/x86_64/monitor_gdb_arch.mk

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)
