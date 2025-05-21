include $(REP_DIR)/lib/mk/libc.mk

INC_DIR += $(LIBC_DIR)/include/spec/arm

vpath % $(REP_DIR)/src/lib/libc/spec/arm

CC_CXX_WARN_STRICT =
