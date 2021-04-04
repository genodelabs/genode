SANITIZER_PORT_DIR := $(call select_from_ports,sanitizer)

SANITIZER_DIR := $(SANITIZER_PORT_DIR)/src/lib/sanitizer

SRC_CC = sanitizer_allocator.cpp \
         sanitizer_common.cc \
         sanitizer_common_libcdep.cpp \
         sanitizer_coverage_libcdep_new.cpp \
         sanitizer_file.cpp \
         sanitizer_flag_parser.cpp \
         sanitizer_flags.cpp \
         sanitizer_genode.cc \
         sanitizer_libc.cpp \
         sanitizer_persistent_allocator.cpp \
         sanitizer_printf.cpp \
         sanitizer_stackdepot.cpp \
         sanitizer_stacktrace.cpp \
         sanitizer_stacktrace_libcdep.cpp \
         sanitizer_stacktrace_printer.cpp \
         sanitizer_suppressions.cpp \
         sanitizer_symbolizer.cpp \
         sanitizer_symbolizer_libbacktrace.cpp \
         sanitizer_symbolizer_libcdep.cpp \
         sanitizer_symbolizer_posix_libcdep.cpp \
         sanitizer_symbolizer_report.cpp \
         sanitizer_termination.cpp

INC_DIR += $(SANITIZER_DIR)

vpath %.cc  $(SANITIZER_DIR)/sanitizer_common
vpath %.cpp $(SANITIZER_DIR)/sanitizer_common

SHARED_LIB = yes

CC_CXX_WARN_STRICT =
