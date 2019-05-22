include $(REP_DIR)/lib/mk/spark.inc

ADALIB     = $(ADA_RTS)/adalib
ADAINCLUDE = $(ADA_RTS)/adainclude

SRC_ADS += system.ads \
	   s-soflin.ads \
	   s-unstyp.ads \
	   interfac.ads \
	   i-cexten.ads \
	   a-except.ads \
	   gnat.ads \
	   ada.ads \
	   ada_exceptions.ads

SRC_ADB += g-io.adb s-stalib.adb s-secsta.adb s-parame.adb i-c.adb s-arit64.adb s-stoele.adb
CUSTOM_ADA_FLAGS = --RTS=$(ADA_RTS) -c -gnatg -gnatp -gnatpg -gnatn2

# C runtime glue code
SRC_CC += genode.cc
SRC_C += init.c

# Ada packages that implement runtime functionality
SRC_ADB += ss_utils.adb string_utils.adb platform.adb s-init.adb

vpath %.cc  $(ADA_RUNTIME_PLATFORM_DIR)

vpath s-soflin.ads $(ADA_RUNTIME_DIR)
vpath a-except.ads $(ADA_RUNTIME_DIR)

vpath s-secsta.adb $(ADA_RUNTIME_DIR)
vpath s-soflin.adb $(ADA_RUNTIME_DIR)
vpath s-stalib.adb $(ADA_RUNTIME_DIR)
vpath s-parame.adb $(ADA_RUNTIME_DIR)
vpath a-except.adb $(ADA_RUNTIME_DIR)
vpath i-c.adb $(ADA_RUNTIME_DIR)

vpath %.ads $(ADA_RTS_SOURCE)
vpath %.adb $(ADA_RTS_SOURCE)

vpath platform.% $(ADA_RUNTIME_LIB_DIR)
vpath string_utils.% $(ADA_RUNTIME_LIB_DIR)
vpath ss_utils.% $(ADA_RUNTIME_LIB_DIR)
vpath ada_exceptions.ads $(ADA_RUNTIME_LIB_DIR)
vpath init.c $(ADA_RUNTIME_LIB_DIR)
vpath s-init.adb $(ADA_RUNTIME_COMMON_DIR)

SHARED_LIB = yes
