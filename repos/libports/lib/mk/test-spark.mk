LIBS    += base spark
SRC_ADB += add_package.adb

include $(REP_DIR)/lib/import/import-test-spark.mk

vpath %.adb $(REP_DIR)/src/test/spark/lib
