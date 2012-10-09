TARGET   = acpi_drv
REQUIRES = x86_32
SRC_CC   = main.cc acpi.cc
LIBS     = cxx env server child

INC_DIR  = $(PRG_DIR)/..

vpath main.cc $(PRG_DIR)/..
vpath acpi.cc $(PRG_DIR)/..
