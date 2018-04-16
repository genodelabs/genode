
include $(REP_DIR)/lib/import/import-ada.mk
ADALIB = $(ADA_RTS)/adalib
ADAINCLUDE = $(ADA_RTS)/adainclude

PACKAGES = system s-stoele s-secsta

body_exists := $(filter $1.adb,$(shell if [ -e $2/$1.adb ]; then echo $1.adb; fi))

ADA_RTS_SOURCE = $(call select_from_ports,gcc)/src/noux-pkg/gcc/gcc/ada
SRC_ADS += $(foreach package, $(PACKAGES), $(package).ads)
SRC_ADB += $(foreach package, $(PACKAGES), $(body_exists, $(package), $(ADA_RTS_SOURCE)))
SRC_ADB += $(foreach package, $(PACKAGES), $(body_exists, $(package), $(REP_DIR)/src/lib/ada))

CUSTOM_ADA_MAKE    = $(CC)
CUSTOM_ADA_FLAGS   = -c -gnatg -gnatp -gnatpg -gnatn2
CUSTOM_ADA_OPT     = $(CC_ADA_OPT)
CUSTOM_ADA_INCLUDE = -I- -I$(REP_DIR)/src/lib/ada -I$(ADA_RTS_SOURCE) -I$(REP_DIR)/src/lib/ada/libsrc

SRC_CC += a-except.cc s-soflin.cc
SRC_CC += c-secsta.cc gnat_except.cc
SRC_ADB += ss_utils.adb

vpath %.cc $(REP_DIR)/src/lib/ada

vpath %.adb $(REP_DIR)/src/lib/ada $(ADA_RTS_SOURCE) $(REP_DIR)/src/lib/ada/libsrc
vpath %.ads $(REP_DIR)/src/lib/ada $(ADA_RTS_SOURCE)

SHARED_LIB = yes

all: ada_source_path ada_object_path

ada_source_path: ada_object_path
	$(VERBOSE)$(shell echo $(CONTRIB_DIR)/gcc-$(shell echo $(call _hash_of_port,gcc) | cut -d" " -f1)/src/noux-pkg/gcc/gcc/ada > $(ADA_RTS)/ada_source_path)

ada_object_path:
	$(VERBOSE)$(shell mkdir -p $(ADA_RTS))
	$(VERBOSE)$(shell echo $(ADA_RTS) > $(ADA_RTS)/ada_object_path)
