REQUIRES = arm

include $(REP_DIR)/lib/mk/ldso.inc

INC_DIR += $(DIR)/arm
vpath %.s $(DIR)/arm
