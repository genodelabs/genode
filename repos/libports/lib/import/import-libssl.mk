REP_INC_DIR += include/openssl

ifeq ($(filter-out $(SPECS),x86_32),)
TARGET_CPUARCH=x86_32
else ifeq ($(filter-out $(SPECS),x86_64),)
TARGET_CPUARCH=x86_64
endif

# include architecture specific opensslconf.h
REP_INC_DIR += src/lib/openssl/$(TARGET_CPUARCH)
