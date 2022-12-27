CCID_DIR := $(call port_dir,$(REP_DIR)/ports/ccid)
PCSC_DIR := $(call port_dir,$(REP_DIR)/ports/pcsc-lite)

MIRROR_FROM_CCID_DIR := \
	$(shell \
		cd $(CCID_DIR); \
		find src/lib/ccid -type f | \
		grep -v "\.git")

MIRROR_FROM_PCSC_DIR := \
	$(shell cd $(PCSC_DIR); find include -type f) \
	$(shell \
		cd $(PCSC_DIR); \
		find src/lib/pcsc-lite -type f | \
		grep -v "\.git") \

MIRROR_FROM_REP_DIR += \
	lib/mk/pcsc-lite.mk \
	lib/mk/ccid.mk \
	$(shell cd $(REP_DIR) && find src/lib/pcsc-lite -type f) \
	$(shell cd $(REP_DIR) && find src/lib/ccid -type f)

MIRROR_FROM_CCID_DIR := \
	$(filter-out $(MIRROR_FROM_REP_DIR), $(MIRROR_FROM_CCID_DIR))

MIRROR_FROM_PCSC_DIR := \
	$(filter-out $(MIRROR_FROM_REP_DIR), $(MIRROR_FROM_PCSC_DIR))

content: $(MIRROR_FROM_CCID_DIR)

$(MIRROR_FROM_CCID_DIR):
	mkdir -p $(dir $@)
	cp -r $(CCID_DIR)/$@ $(dir $@)

content: $(MIRROR_FROM_PCSC_DIR)

$(MIRROR_FROM_PCSC_DIR):
	mkdir -p $(dir $@)
	cp -r $(PCSC_DIR)/$@ $(dir $@)

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

content: LICENSE

LICENSE:
	echo "" >> $@
	echo "ccid library" >> $@
	echo "############" >> $@
	echo "" >> $@
	cat $(CCID_DIR)/src/lib/ccid/COPYING >> $@
	echo "" >> $@
	echo "pcsc-lite library" >> $@
	echo "#################" >> $@
	echo "" >> $@
	cat $(PCSC_DIR)/src/lib/pcsc-lite/COPYING >> $@
