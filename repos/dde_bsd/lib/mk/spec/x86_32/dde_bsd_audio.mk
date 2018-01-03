INC_DIR += $(LIB_INC_DIR)/spec/x86_32 $(LIB_INC_DIR)/spec/x86

include $(REP_DIR)/lib/mk/dde_bsd_audio.inc

vpath %.S $(LIB_DIR)/spec/x86_32

CC_CXX_WARN_STRICT =
