REQUIRES := x86

LIBS += base acpica config

CC_OPT += -Wno-unused-function -Wno-unused-variable
CC_C_OPT += -DACPI_LIBRARY

INC_DIR += $(call select_from_ports,acpica)/src/lib/acpica/source/include
