include $(REP_DIR)/lib/mk/spark.inc

ADALIB     = $(ADA_RTS)/adalib
ADAINCLUDE = $(ADA_RTS)/adainclude

SRC_ADS += system.ads \
	   s-soflin.ads \
	   s-imgint.ads \
	   s-stoele.ads \
	   s-unstyp.ads \
	   interfac.ads \
	   a-except.ads \
	   gnat.ads \
	   ada.ads \
	   ada_exceptions.ads

SRC_ADB += g-io.adb s-stalib.adb s-secsta.adb s-parame.adb
CUSTOM_ADA_FLAGS = --RTS=$(ADA_RTS) -c -gnatg -gnatp -gnatpg -gnatn2

# C runtime glue code
SRC_CC += genode.cc

# Ada packages that implement runtime functionality
SRC_ADB += ss_utils.adb string_utils.adb platform.adb

vpath %.cc  $(ADA_RUNTIME_PLATFORM_DIR)

vpath s-soflin.ads $(ADA_RUNTIME_DIR)
vpath a-except.ads $(ADA_RUNTIME_DIR)

vpath s-secsta.adb $(ADA_RUNTIME_DIR)
vpath s-soflin.adb $(ADA_RUNTIME_DIR)
vpath s-stalib.adb $(ADA_RUNTIME_DIR)
vpath s-parame.adb $(ADA_RUNTIME_DIR)
vpath a-except.adb $(ADA_RUNTIME_DIR)

vpath %.ads $(ADA_RTS_SOURCE)
vpath %.adb $(ADA_RTS_SOURCE)

vpath platform.% $(ADA_RUNTIME_LIB_DIR)
vpath string_utils.% $(ADA_RUNTIME_LIB_DIR)
vpath ss_utils.% $(ADA_RUNTIME_LIB_DIR)
vpath ada_exceptions.ads $(ADA_RUNTIME_LIB_DIR)

SHARED_LIB = yes
