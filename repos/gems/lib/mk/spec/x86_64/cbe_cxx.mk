CBE_DIR := $(call select_from_ports,cbe)

LIBS += spark libsparkcrypto sha256_4k cbe cbe_common cbe_cxx_common

INC_DIR += $(CBE_DIR)/src/lib/cbe
INC_DIR += $(CBE_DIR)/src/lib/cbe_common
INC_DIR += $(CBE_DIR)/src/lib/cbe_cxx
INC_DIR += $(CBE_DIR)/src/lib/cbe_cxx_common

SRC_ADB += cbe-cxx-cxx_library.adb

vpath % $(CBE_DIR)/src/lib/cbe_cxx

SHARED_LIB := yes

include $(REP_DIR)/lib/mk/generate_ada_main_pkg.inc
