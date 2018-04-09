
RTS_DIR = $(BUILD_BASE_DIR)/var/libcache/ada
ADALIB = $(RTS_DIR)/adalib
ADAINCLUDE = $(RTS_DIR)/adainclude

include $(REP_DIR)/lib/import/import-ada.mk

PACKAGES = system

ADA_RTS_SOURCE = $(call select_from_ports,gcc)/src/noux-pkg/gcc/gcc/ada
SRC_ADS += $(foreach package, $(PACKAGES), $(package).ads)

vpath %.ads $(ADA_RTS_SOURCE)

SHARED_LIB = yes

all: ada_source_path ada_object_path

ada_source_path: ada_object_path
	$(VERBOSE)$(shell echo $(CONTRIB_DIR)/gcc-$(shell echo $(call _hash_of_port,gcc) | cut -d" " -f1)/src/noux-pkg/gcc/gcc/ada > $(RTS_DIR)/ada_source_path)

ada_object_path:
	$(VERBOSE)$(shell mkdir -p $(RTS_DIR))
	$(VERBOSE)$(shell echo $(RTS_DIR) > $(RTS_DIR)/ada_object_path)
