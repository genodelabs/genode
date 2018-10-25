
include $(REP_DIR)/lib/mk/ada.inc

ADALIB     = $(ADA_RTS)/adalib
ADAINCLUDE = $(ADA_RTS)/adainclude

PACKAGES = system s-stoele s-secsta a-except s-conca2 s-arit64

body_exists := $(filter $1.adb,$(shell if [ -e $2/$1.adb ]; then echo $1.adb; fi))

SRC_ADS += $(foreach package, $(PACKAGES), $(package).ads)
SRC_ADB += $(foreach package, $(PACKAGES), $(body_exists, $(package), $(ADA_RTS_SOURCE)))
SRC_ADB += $(foreach package, $(PACKAGES), $(body_exists, $(package), $(REP_DIR)/src/lib/ada/runtime))

CUSTOM_ADA_MAKE    = $(CC)
CUSTOM_ADA_FLAGS   = -c -gnatg -gnatp -gnatpg -gnatn2
CUSTOM_ADA_OPT     = $(CC_ADA_OPT)
CUSTOM_ADA_INCLUDE = -I- -I$(REP_DIR)/src/lib/ada/runtime -I$(ADA_RTS_SOURCE) -I$(REP_DIR)/src/lib/ada/runtimelib

# pure C runtime implementations
SRC_CC += a-except_c.cc s-soflin_c.cc a-exctab_c.cc

# C runtime glue code
SRC_CC += s-secsta_c.cc libc.cc

# Ada packages that implement runtime functionality
SRC_ADB += ss_utils.adb

vpath %.cc $(REP_DIR)/src/lib/ada/runtimelib
vpath %.adb $(REP_DIR)/src/lib/ada/runtime $(ADA_RTS_SOURCE) $(REP_DIR)/src/lib/ada/runtimelib
vpath %.ads $(REP_DIR)/src/lib/ada/runtime $(ADA_RTS_SOURCE)

SHARED_LIB = yes
