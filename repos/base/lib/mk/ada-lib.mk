LIBS += base ada
SRC_ADA += add_package.adb

include $(REP_DIR)/lib/import/import-ada-lib.mk

vpath %.adb $(REP_DIR)/src/test/ada-lib
