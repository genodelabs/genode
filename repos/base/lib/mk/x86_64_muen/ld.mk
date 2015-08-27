REQUIRES = x86 64bit

include $(REP_DIR)/lib/mk/ldso.inc

INC_DIR += $(DIR)/x86_64
vpath %.s $(DIR)/x86_64
