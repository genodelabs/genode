CBE_DIR := $(call select_from_ports,cbe)

SRC_ADB := sha256_4k.adb
LIBS    += spark libsparkcrypto

CC_ADA_OPT += -gnatec=$(CBE_DIR)/src/lib/sha256_4k/spark.adc

INC_DIR += $(CBE_DIR)/src/lib/sha256_4k

sha256_4k.o : $(CBE_DIR)/src/lib/sha256_4k/sha256_4k.ads

vpath % $(CBE_DIR)/src/lib/sha256_4k
