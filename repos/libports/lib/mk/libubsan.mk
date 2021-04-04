SANITIZER_PORT_DIR := $(call select_from_ports,sanitizer)

SANITIZER_DIR := $(SANITIZER_PORT_DIR)/src/lib/sanitizer

SRC_CC = ubsan_diag.cpp \
         ubsan_flags.cpp \
         ubsan_handlers.cpp \
         ubsan_handlers_cxx.cpp \
         ubsan_init.cpp \
         ubsan_monitor.cpp \
         ubsan_type_hash.cpp \
         ubsan_type_hash_itanium.cpp \
         ubsan_value.cpp

CC_OPT += -DCAN_SANITIZE_UB=1 -DUBSAN_CAN_USE_CXXABI=1

LIBS += libsanitizer_common

INC_DIR += $(SANITIZER_DIR)

vpath %.cpp $(SANITIZER_DIR)/ubsan

SHARED_LIB = yes

CC_CXX_WARN_STRICT =
