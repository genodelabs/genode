REQUIRES := x86

ACPICA_DIR := $(call select_from_ports,acpica)/src/lib/acpica
ACPICA_COMP := $(ACPICA_DIR)/source/components

SRC_C += debugger/dbdisply.c debugger/dbobject.c debugger/dbxface.c
SRC_C += $(addprefix disassembler/, $(notdir $(wildcard $(ACPICA_COMP)/disassembler/*.c)))
SRC_C += $(addprefix dispatcher/, $(notdir $(wildcard $(ACPICA_COMP)/dispatcher/*.c)))
SRC_C += $(addprefix events/, $(notdir $(wildcard $(ACPICA_COMP)/events/*.c)))
SRC_C += $(addprefix executer/, $(notdir $(wildcard $(ACPICA_COMP)/executer/*.c)))
SRC_C += $(addprefix hardware/, $(notdir $(wildcard $(ACPICA_COMP)/hardware/*.c)))
SRC_C += $(addprefix namespace/, $(notdir $(wildcard $(ACPICA_COMP)/namespace/*.c)))
SRC_C += $(addprefix parser/, $(notdir $(wildcard $(ACPICA_COMP)/parser/*.c)))
SRC_C += $(addprefix resources/, $(notdir $(wildcard $(ACPICA_COMP)/resources/*.c)))
SRC_C += $(addprefix tables/, $(notdir $(wildcard $(ACPICA_COMP)/tables/*.c)))
SRC_C += $(addprefix utilities/, $(notdir $(wildcard $(ACPICA_COMP)/utilities/*.c)))

SRC_CC += osl.cc iomem.cc pci.cc env.cc

include $(REP_DIR)/lib/import/import-acpica.mk

CC_C_OPT += -DACPI_LIBRARY

vpath %.c $(ACPICA_COMP)
vpath %.cc $(REP_DIR)/src/lib/acpica

CC_CXX_WARN_STRICT =
