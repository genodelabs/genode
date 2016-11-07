INC_DIR += $(call select_from_ports,curl)/include

ifeq ($(filter-out $(SPECS),32bit),)
TARGET_CPUARCH=32bit
else ifeq ($(filter-out $(SPECS),64bit),)
TARGET_CPUARCH=64bit
endif

# include architecture specific curlbuild.h
REP_INC_DIR += src/lib/curl/spec/$(TARGET_CPUARCH)
REP_INC_DIR += src/lib/curl/spec/$(TARGET_CPUARCH)/curl

REP_INC_DIR += src/lib/curl
