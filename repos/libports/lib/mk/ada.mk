
include $(REP_DIR)/lib/mk/ada.inc

ADALIB     = $(ADA_RTS)/adalib
ADAINCLUDE = $(ADA_RTS)/adainclude

ADA_RTS_SOURCE = $(call select_from_ports,ada-runtime)/ada-runtime/contrib/gcc-6.3.0
ADA_RUNTIME_DIR = $(call select_from_ports,ada-runtime)/ada-runtime/src
ADA_RUNTIME_LIB_DIR = $(call select_from_ports,ada-runtime)/ada-runtime/src/lib
ADA_RUNTIME_PLATFORM_DIR = $(call select_from_ports,ada-runtime)/ada-runtime/platform

SRC_ADS += system.ads \
	   s-soflin.ads \
	   s-imgint.ads \
	   s-stoele.ads \
	   s-secsta.ads \
	   interfac.ads \
	   a-except.ads \
	   gnat.ads

SRC_ADB += g-io.adb

CUSTOM_ADA_MAKE    = $(CC)
CUSTOM_ADA_FLAGS   = -c -gnatg -gnatp -gnatpg -gnatn2
CUSTOM_ADA_OPT     = $(CC_ADA_OPT)
CUSTOM_ADA_INCLUDE = -I- -I$(ADA_RUNTIME_DIR) -I$(ADA_RTS_SOURCE) -I$(ADA_RUNTIME_LIB_DIR)

INC_DIR += $(ADA_RUNTIME_LIB_DIR)

# C runtime glue code
SRC_CC += genode.cc

# Ada packages that implement runtime functionality
SRC_ADB += ss_utils.adb string_utils.adb platform.adb

vpath %.cc  $(ADA_RUNTIME_PLATFORM_DIR)

vpath system.ads $(ADA_RTS_SOURCE)
vpath s-soflin.ads $(ADA_RUNTIME_DIR)
vpath s-stoele.ads $(ADA_RTS_SOURCE)
vpath s-secsta.ads $(ADA_RUNTIME_DIR)
vpath s-imgint.ads $(ADA_RTS_SOURCE)
vpath a-except.ads $(ADA_RUNTIME_DIR)
vpath interfac.ads $(ADA_RTS_SOURCE)
vpath gnat.ads     $(ADA_RTS_SOURCE)
vpath g-io.ads     $(ADA_RTS_SOURCE)

vpath s-stoele.adb $(ADA_RTS_SOURCE)
vpath s-secsta.adb $(ADA_RUNTIME_DIR)
vpath s-soflin.adb $(ADA_RUNTIME_DIR)
vpath s-imgint.adb $(ADA_RTS_SOURCE)
vpath a-except.adb $(ADA_RUNTIME_DIR)
vpath g-io.adb     $(ADA_RTS_SOURCE)

vpath platform.% $(ADA_RUNTIME_LIB_DIR)
vpath string_utils.% $(ADA_RUNTIME_LIB_DIR)
vpath ss_utils.% $(ADA_RUNTIME_LIB_DIR)

SHARED_LIB = yes
