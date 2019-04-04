SANITIZER_PORT_DIR := $(call select_from_ports,sanitizer)

SANITIZER_DIR := $(SANITIZER_PORT_DIR)/src/lib/sanitizer

SRC_CC = ubsan_diag.cc \
         ubsan_flags.cc \
         ubsan_handlers.cc \
         ubsan_handlers_cxx.cc \
         ubsan_init.cc \
         ubsan_type_hash.cc \
         ubsan_type_hash_itanium.cc \
         ubsan_value.cc

CC_OPT += -DCAN_SANITIZE_UB=1 -DUBSAN_CAN_USE_CXXABI=1

LIBS += libsanitizer_common

INC_DIR += $(SANITIZER_DIR)

vpath %.cc $(SANITIZER_DIR)/ubsan

SHARED_LIB = yes

CC_CXX_WARN_STRICT =
