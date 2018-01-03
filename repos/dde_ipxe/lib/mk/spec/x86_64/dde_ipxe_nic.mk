include $(REP_DIR)/lib/mk/dde_ipxe_nic.inc

INC_DIR += $(IPXE_CONTRIB_DIR)/arch/x86_64/include \
           $(IPXE_CONTRIB_DIR)/arch/x86_64/include/efi

# take remaining parts from i386
INC_DIR += $(IPXE_CONTRIB_DIR)/arch/i386/include

CC_CXX_WARN_STRICT =
