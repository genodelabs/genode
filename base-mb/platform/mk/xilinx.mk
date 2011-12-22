#
# \brief  Configure the supported Xilinx FPGAs
# \author Martin Stein
# \date   2011-05-23
#

TMP_FILES       += $(WORK_DIR)/_impactbatch.log
CLEANLOCK_IMPACT = $(WORK_DIR)/cleanlock.impact

configure: $(CONFIGURE_IMPACT) cleanlock
	$(VERBOSE) impact -batch $(CONFIGURE_IMPACT)
	$(VERBOSE) make clean

cleanlock: $(CLEANLOCK_IMPACT)
	$(VERBOSE) impact -batch $(CLEANLOCK_IMPACT) || true

$(CLEANLOCK_IMPACT):
	$(VERBOSE) echo \
		"cleancablelock"\
		"\nexit"\
	> $@

.INTERMEDIATE: $(UPLOAD_XMD) $(CLEANLOCK_IMPACT)
.PHONY: configure cleanlock
