TARGET   = acpi_drv
REQUIRES = x86
SRC_CC   = main.cc acpi.cc
LIBS     = base

INC_DIR  = $(PRG_DIR)/..

vpath main.cc $(PRG_DIR)/..
vpath acpi.cc $(PRG_DIR)/..
