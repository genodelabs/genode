ADA_RTS = $(BUILD_BASE_DIR)/var/libcache/spark

ADA_RTS_SOURCE = $(call select_from_ports,ada-runtime)/ada-runtime/contrib/gcc-8.3.0
ADA_RUNTIME_DIR = $(call select_from_ports,ada-runtime)/ada-runtime/src/minimal
ADA_RUNTIME_LIB_DIR = $(call select_from_ports,ada-runtime)/ada-runtime/src/lib
ADA_RUNTIME_COMMON_DIR = $(call select_from_ports,ada-runtime)/ada-runtime/src/common
ADA_RUNTIME_PLATFORM_DIR = $(call select_from_ports,ada-runtime)/ada-runtime/platform

INC_DIR += $(ADA_RUNTIME_DIR) \
	   $(ADA_RUNTIME_LIB_DIR) \
	   $(ADA_RUNTIME_COMMON_DIR) \
	   $(ADA_RTS_SOURCE) \
	   $(ADA_RUNTIME_PLATFORM_DIR)

# Disable inline concatenation as this requires additinal runtime support
CC_ADA_OPT += -gnatd.c
