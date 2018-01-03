include $(REP_DIR)/lib/mk/wifi.inc

INC_DIR += $(LIB_INC_DIR)/spec/64bit $(LIB_INC_DIR)/spec/x86_64 $(LIB_INC_DIR)/spec/x86

vpath %.S  $(REP_DIR)/src/lx_kit/spec/x86_64

CC_CXX_WARN_STRICT =
