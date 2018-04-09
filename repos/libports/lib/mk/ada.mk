
include $(REP_DIR)/lib/import/import-ada.mk
ADALIB = $(ADA_RTS)/adalib
ADAINCLUDE = $(ADA_RTS)/adainclude

PACKAGES = system

ADA_RTS_SOURCE = $(call select_from_ports,gcc)/src/noux-pkg/gcc/gcc/ada
SRC_ADS += $(foreach package, $(PACKAGES), $(package).ads)

vpath %.ads $(ADA_RTS_SOURCE)

SHARED_LIB = yes

all: ada_source_path ada_object_path

ada_source_path: ada_object_path
	$(VERBOSE)$(shell echo $(CONTRIB_DIR)/gcc-$(shell echo $(call _hash_of_port,gcc) | cut -d" " -f1)/src/noux-pkg/gcc/gcc/ada > $(ADA_RTS)/ada_source_path)

ada_object_path:
	$(VERBOSE)$(shell mkdir -p $(ADA_RTS))
	$(VERBOSE)$(shell echo $(ADA_RTS) > $(ADA_RTS)/ada_object_path)
