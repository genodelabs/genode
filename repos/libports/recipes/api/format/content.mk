MIRROR_FROM_REP_DIR := include/format src/lib/format lib/mk/format.mk

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

MIRROR_FROM_BASE_DIR := src/include/base/internal/output.h

content: $(MIRROR_FROM_BASE_DIR)

$(MIRROR_FROM_BASE_DIR):
	mkdir -p $(dir $@)
	cp -r $(addprefix $(GENODE_DIR)/repos/base/,$@) $(dir $@)

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@
