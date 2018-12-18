TARGET   = test-ada
SRC_ADB  = main.adb machinery.adb
SRC_CC   = print.cc startup.cc
LIBS     = base ada test-ada

CC_ADA_OPT += -gnatec=$(PRG_DIR)/spark.adc

INC_DIR += $(PRG_DIR)

machinery.o : machinery.ads
