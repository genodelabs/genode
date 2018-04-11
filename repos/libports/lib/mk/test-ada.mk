LIBS    += base ada
SRC_ADB += add_package.adb

include $(REP_DIR)/lib/import/import-test-ada.mk

vpath %.adb $(REP_DIR)/src/test/ada/lib
