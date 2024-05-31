MIRROR_FROM_REP_DIR := \
	src/driver/nic/include \
	lib/mk/nic_driver.mk \
	lib/import/import-nic_driver.mk

content: $(MIRROR_FROM_REP_DIR) LICENSE

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@
