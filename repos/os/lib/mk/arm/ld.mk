REQUIRES   = arm
SHARED_LIB = yes
DIR        = $(REP_DIR)/src/lib/ldso

INC_DIR += $(DIR)/contrib/arm \
           $(DIR)/include/libc/libc-arm \
           $(DIR)/include/arm

vpath platform.c $(DIR)/arm
vpath %          $(DIR)/contrib/arm

include $(DIR)/target.inc
