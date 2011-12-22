#
# \brief  Configure support for the Xilinx ML507 Development Kit via JTAG
# \author Martin Stein
# \date   2011-05-23
#

$(CONFIGURE_IMPACT):
	$(VERBOSE) echo \
		"setMode -bscan"\
		"\nsetCable -p auto"\
		"\nidentify"\
		"\nassignfile -p 5 -file $(BIT_FILE)"\
		"\nprogram -p 5"\
		"\nquit"\
	> $@

.INTERMEDIATE: $(CONFIGURE_IMPACT)

include $(MK_DIR)/xilinx.mk
