REQUIRES = x86 32bit

include $(REP_DIR)/lib/mk/ldso.inc

INC_DIR += $(DIR)/spec/x86_32
vpath %.s $(DIR)/spec/x86_32
