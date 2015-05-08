INC_DIR += $(LIB_INC_DIR)/x86_64 $(LIB_INC_DIR)/x86

include $(REP_DIR)/lib/mk/dde_bsd_audio.inc

vpath %.S $(LIB_DIR)/x86_64
