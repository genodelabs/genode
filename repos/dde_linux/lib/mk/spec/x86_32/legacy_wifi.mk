include $(REP_DIR)/lib/mk/legacy_wifi.inc

INC_DIR += $(LIB_INC_DIR)/spec/32bit $(LIB_INC_DIR)/spec/x86_32 $(LIB_INC_DIR)/spec/x86

CC_OPT += -DCONFIG_64BIT=0

vpath %.S  $(REP_DIR)/src/lib/legacy/lx_kit/spec/x86_32

CC_CXX_WARN_STRICT =
