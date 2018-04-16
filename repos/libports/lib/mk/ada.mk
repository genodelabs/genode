
include $(REP_DIR)/lib/import/import-ada.mk
ADALIB = $(ADA_RTS)/adalib
ADAINCLUDE = $(ADA_RTS)/adainclude

PACKAGES = system s-stoele s-secsta

body_exists := $(filter $1.adb,$(shell if [ -e $2/$1.adb ]; then echo $1.adb; fi))

ADA_RTS_SOURCE = $(call select_from_ports,gcc)/src/noux-pkg/gcc/gcc/ada
SRC_ADS += $(foreach package, $(PACKAGES), $(package).ads)
SRC_ADB += $(foreach package, $(PACKAGES), $(body_exists, $(package), $(ADA_RTS_SOURCE)))
SRC_ADB += $(foreach package, $(PACKAGES), $(body_exists, $(package), $(REP_DIR)/src/lib/ada/runtime))

CUSTOM_ADA_MAKE    = $(CC)
CUSTOM_ADA_FLAGS   = -c -gnatg -gnatp -gnatpg -gnatn2
CUSTOM_ADA_OPT     = $(CC_ADA_OPT)
CUSTOM_ADA_INCLUDE = -I- -I$(REP_DIR)/src/lib/ada/runtime -I$(ADA_RTS_SOURCE) -I$(REP_DIR)/src/lib/ada/runtimelib

# pure C runtime implementations
SRC_CC += a-except_c.cc s-soflin_c.cc

# C runtime glue code
SRC_CC += s-secsta_c.cc gnat_except.cc

# Ada packages that implement runtime functionality
SRC_ADB += ss_utils.adb

vpath %.cc $(REP_DIR)/src/lib/ada/runtimelib
vpath %.adb $(REP_DIR)/src/lib/ada/runtime $(ADA_RTS_SOURCE) $(REP_DIR)/src/lib/ada/runtimelib
vpath %.ads $(REP_DIR)/src/lib/ada/runtime $(ADA_RTS_SOURCE)

SHARED_LIB = yes

all: ada_source_path ada_object_path

ada_source_path: ada_object_path
	$(VERBOSE)$(shell echo $(CONTRIB_DIR)/gcc-$(shell echo $(call _hash_of_port,gcc) | cut -d" " -f1)/src/noux-pkg/gcc/gcc/ada > $(ADA_RTS)/ada_source_path)

ada_object_path:
	$(VERBOSE)$(shell mkdir -p $(ADA_RTS))
	$(VERBOSE)$(shell echo $(ADA_RTS) > $(ADA_RTS)/ada_object_path)
