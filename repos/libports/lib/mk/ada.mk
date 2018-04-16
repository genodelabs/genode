
include $(REP_DIR)/lib/import/import-ada.mk
ADALIB = $(ADA_RTS)/adalib
ADAINCLUDE = $(ADA_RTS)/adainclude

PACKAGES = system

body_exists := $(filter $1.adb,$(shell if [ -e $(ADA_RTS_SOURCE)/$1.adb ]; then echo $1.adb; fi))

ADA_RTS_SOURCE = $(call select_from_ports,gcc)/src/noux-pkg/gcc/gcc/ada
SRC_ADS += $(foreach package, $(PACKAGES), $(package).ads)
SRC_ADB += $(foreach package, $(PACKAGES), $(body_exists, $(package)))

CUSTOM_ADA_MAKE    = $(CC)
CUSTOM_ADA_FLAGS   = -c -gnatg -gnatp -gnatpg -gnatn2
CUSTOM_ADA_OPT     = $(CC_ADA_OPT)
CUSTOM_ADA_INCLUDE = -I- -I$(ADA_RTS_SOURCE)

vpath %.adb $(ADA_RTS_SOURCE)
vpath %.ads $(ADA_RTS_SOURCE)

SHARED_LIB = yes

all: ada_source_path ada_object_path

ada_source_path: ada_object_path
	$(VERBOSE)$(shell echo $(CONTRIB_DIR)/gcc-$(shell echo $(call _hash_of_port,gcc) | cut -d" " -f1)/src/noux-pkg/gcc/gcc/ada > $(ADA_RTS)/ada_source_path)

ada_object_path:
	$(VERBOSE)$(shell mkdir -p $(ADA_RTS))
	$(VERBOSE)$(shell echo $(ADA_RTS) > $(ADA_RTS)/ada_object_path)
