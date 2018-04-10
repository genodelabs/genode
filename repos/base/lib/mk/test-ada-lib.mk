LIBS += base ada
SRC_ADB += add_package.adb

include $(REP_DIR)/lib/import/import-test-ada-lib.mk

vpath %.adb $(REP_DIR)/src/test/test-ada-lib
