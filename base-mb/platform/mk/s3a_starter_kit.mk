#
# \brief  Configure support for the Xilinx Spartan 3A Starter Kit via JTAG
# \author Martin Stein
# \date   2011-05-23
#

CONFIGURE_IMPACT = $(WORK_DIR)/configure.impact

$(CONFIGURE_IMPACT):
	$(VERBOSE) echo \
		"\nsetMode -bs"\
		"\nsetCable -port auto"\
		"\nIdentify -inferir"\
		"\nidentifyMPM"\
		"\nassignFile -p 1 -file \"$(BIT_FILE)\""\
		"\nProgram -p 1"\
		"\nquit"\
	> $@

.INTERMEDIATE: $(CONFIGURE_IMPACT)

include $(MK_DIR)/xilinx.mk
