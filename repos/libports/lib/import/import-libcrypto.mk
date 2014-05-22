INC_DIR += $(call select_from_ports,openssl)/include

ifeq ($(filter-out $(SPECS),x86_32),)
TARGET_CPUARCH=x86_32
else ifeq ($(filter-out $(SPECS),x86_64),)
TARGET_CPUARCH=x86_64
else ifeq ($(filter-out $(SPECS),arm),)
TARGET_CPUARCH=arm
endif

# include architecture specific opensslconf.h
REP_INC_DIR += src/lib/openssl/$(TARGET_CPUARCH)
