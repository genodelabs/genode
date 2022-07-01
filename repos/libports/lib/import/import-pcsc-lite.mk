ifeq ($(CONTRIB_DIR),)
REP_INC_DIR += include/PCSC
else
INC_DIR += $(call select_from_ports,pcsc-lite)/include/PCSC
endif
