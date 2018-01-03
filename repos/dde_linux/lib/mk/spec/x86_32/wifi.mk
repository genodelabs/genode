include $(REP_DIR)/lib/mk/wifi.inc

INC_DIR += $(LIB_INC_DIR)/spec/32bit $(LIB_INC_DIR)/spec/x86_32 $(LIB_INC_DIR)/spec/x86

vpath %.S  $(REP_DIR)/src/lx_kit/spec/x86_32

CC_CXX_WARN_STRICT =
