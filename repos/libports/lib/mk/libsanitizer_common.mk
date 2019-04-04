SANITIZER_PORT_DIR := $(call select_from_ports,sanitizer)

SANITIZER_DIR := $(SANITIZER_PORT_DIR)/src/lib/sanitizer

SRC_CC = sanitizer_allocator.cc \
         sanitizer_common.cc \
         sanitizer_common_libcdep.cc \
         sanitizer_coverage_libcdep_new.cc \
         sanitizer_file.cc \
         sanitizer_flag_parser.cc \
         sanitizer_flags.cc \
         sanitizer_genode.cc \
         sanitizer_libc.cc \
         sanitizer_persistent_allocator.cc \
         sanitizer_printf.cc \
         sanitizer_stackdepot.cc \
         sanitizer_stacktrace.cc \
         sanitizer_stacktrace_libcdep.cc \
         sanitizer_stacktrace_printer.cc \
         sanitizer_suppressions.cc \
         sanitizer_symbolizer.cc \
         sanitizer_symbolizer_libbacktrace.cc \
         sanitizer_symbolizer_libcdep.cc \
         sanitizer_symbolizer_posix_libcdep.cc \
         sanitizer_termination.cc

INC_DIR += $(SANITIZER_DIR)

vpath %.cc $(SANITIZER_DIR)/sanitizer_common

SHARED_LIB = yes

CC_CXX_WARN_STRICT =
