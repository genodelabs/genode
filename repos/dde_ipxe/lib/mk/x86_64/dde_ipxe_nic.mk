include $(REP_DIR)/lib/mk/dde_ipxe_nic.inc

INC_DIR += $(CONTRIB_DIR)/arch/x86_64/include \
           $(CONTRIB_DIR)/arch/x86_64/include/efi

# take remaining parts from i386
INC_DIR += $(CONTRIB_DIR)/arch/i386/include
