EXFAT_DIR = $(call select_from_ports,exfat)/src/lib/exfat

#FILTER_OUT = win32_io.c
#SRC_C = $(filter-out $(FILTER_OUT),$(notdir $(wildcard $(EXFAT_DIR)/libexfat/*.c)))

SRC_C = $(notdir $(wildcard $(EXFAT_DIR)/libexfat/*.c))

INC_DIR += $(REP_DIR)/include/exfat \
           $(EXFAT_DIR)/libexfat

#CC_OPT += -DHAVE_CONFIG_H -DRECORD_LOCKING_NOT_IMPLEMENTED

LIBS += libc

vpath %.c $(EXFAT_DIR)/libexfat

CC_CXX_WARN_STRICT =
