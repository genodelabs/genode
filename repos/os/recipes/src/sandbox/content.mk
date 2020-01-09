SRC_DIR = src/lib/sandbox
include $(GENODE_DIR)/repos/base/recipes/src/content.inc

MIRROR_FROM_REP_DIR += lib/mk/sandbox.mk
content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

content: src/lib/sandbox/target.mk

src/lib/sandbox/target.mk:
	echo "LIBS += sandbox" > $@
