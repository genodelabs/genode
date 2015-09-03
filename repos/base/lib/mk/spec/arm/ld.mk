REQUIRES = arm

include $(REP_DIR)/lib/mk/ldso.inc

INC_DIR += $(DIR)/spec/arm
vpath %.s $(DIR)/spec/arm
