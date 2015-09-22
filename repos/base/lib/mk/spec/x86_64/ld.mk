REQUIRES = x86 64bit

include $(REP_DIR)/lib/mk/ldso.inc

INC_DIR += $(DIR)/spec/x86_64
vpath %.s $(DIR)/spec/x86_64
