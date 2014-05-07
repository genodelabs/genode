INC_DIR += $(call select_from_ports,curl)/include

ifeq ($(filter-out $(SPECS),x86_32),)
TARGET_CPUARCH=x86_32
else ifeq ($(filter-out $(SPECS),x86_64),)
TARGET_CPUARCH=x86_64
endif

# include architecture specific curlbuild.h
REP_INC_DIR += src/lib/curl/$(TARGET_CPUARCH)
REP_INC_DIR += src/lib/curl/$(TARGET_CPUARCH)/curl

REP_INC_DIR += src/lib/curl
