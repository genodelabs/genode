SRC_ADB := aes_cbc_4k.adb
LIBS    += spark libsparkcrypto

CC_ADA_OPT += -gnatec=$(REP_DIR)/src/lib/aes_cbc_4k/spark.adc

INC_DIR += $(REP_DIR)/src/lib/aes_cbc_4k

aes_cbc_4k.o : aes_cbc_4k.ads

vpath % $(REP_DIR)/src/lib/aes_cbc_4k
