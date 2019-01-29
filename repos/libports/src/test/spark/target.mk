TARGET   = test-spark
SRC_ADB  = main.adb machinery.adb
SRC_CC   = print.cc startup.cc
LIBS     = base spark test-spark

CC_ADA_OPT += -gnatec=$(PRG_DIR)/spark.adc

INC_DIR += $(PRG_DIR)

machinery.o : machinery.ads
