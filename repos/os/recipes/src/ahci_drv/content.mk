SRC_DIR = src/drivers/ahci
include $(GENODE_DIR)/repos/base/recipes/src/content.inc

MIRROR_FROM_REP_DIR := lib/mk/spec/x86/ahci_platform.mk \
                       lib/mk/spec/exynos5/ahci_platform.mk

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

