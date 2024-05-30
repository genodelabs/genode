TARGET   = acpi
REQUIRES = x86
SRC_CC   = main.cc acpi.cc smbios_table_reporter.cc intel_opregion.cc
LIBS     = base

INC_DIR  = $(PRG_DIR)/../..

vpath main.cc                  $(PRG_DIR)/../..
vpath acpi.cc                  $(PRG_DIR)/../..
vpath smbios_table_reporter.cc $(PRG_DIR)/../..
vpath intel_opregion.cc        $(PRG_DIR)/../..

CC_CXX_WARN_STRICT_CONVERSION =
