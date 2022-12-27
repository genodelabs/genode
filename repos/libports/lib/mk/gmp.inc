GMP_DIR := $(call select_from_ports,gmp)/src/lib/gmp

include $(REP_DIR)/lib/import/import-gmp.mk

LIBS += libc

CC_OPT += -DHAVE_CONFIG_H -D__GMP_WITHIN_GMP
CC_OPT += -Wno-unused-variable -Wno-unused-but-set-variable
CC_OPT += -Wno-implicit-function-declaration -Wno-unused-function
CC_OPT += -Wno-unused-value

INC_DIR += $(GMP_DIR)
