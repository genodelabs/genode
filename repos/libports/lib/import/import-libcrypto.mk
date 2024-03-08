LIB_OPENSSL_DIR = $(call select_from_repositories,src/lib/openssl)

OPENSSL_DIR := $(call select_from_ports,openssl)

ARCH = $(filter 32bit 64bit,$(SPECS))

INC_DIR += $(OPENSSL_DIR)/include
INC_DIR += $(LIB_OPENSSL_DIR)/spec/$(ARCH)

LIBS += libc

include $(call select_from_repositories,lib/import/import-libc.mk)
