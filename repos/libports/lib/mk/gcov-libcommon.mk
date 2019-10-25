GCOV_PORT_DIR := $(call select_from_ports,gcov)

GCOV_DIR := $(GCOV_PORT_DIR)/src/gcov

SRC_CC = diagnostic.cc \
         diagnostic-color.cc \
         diagnostic-show-locus.cc \
         edit-context.cc \
         ggc-none.cc \
         hash-table.cc \
         input.cc \
         intl.cc \
         memory-block.cc \
         pretty-print.cc \
         vec.cc \
         version.cc

CC_OPT += -DIN_GCC

CC_OPT_version += -DBASEVER="\"6.3.0\"" \
                  -DDATESTAMP="\"\"" \
                  -DREVISION="\"\"" \
                  -DDEVPHASE="\"\"" \
                  -DPKGVERSION="\"(GCC) \"" \
                  -DBUGURL="\"<http://gcc.gnu.org/bugs.html>\""

LIBS += libc gmp stdcxx

INC_DIR += $(GCOV_DIR)/include \
           $(GCOV_DIR)/libcpp/include

ifeq ($(filter-out $(SPECS),arm),)
	INC_DIR += $(GCOV_PORT_DIR)/include/arm/gcc
endif

ifeq ($(filter-out $(SPECS),arm_64),)
	INC_DIR += $(GCOV_PORT_DIR)/include/arm_64/gcc
endif

ifeq ($(filter-out $(SPECS),x86_32),)
	INC_DIR += $(GCOV_PORT_DIR)/include/x86_32/gcc
endif

ifeq ($(filter-out $(SPECS),x86_64),)
	INC_DIR += $(GCOV_PORT_DIR)/include/x86_64/gcc
endif

vpath %.cc $(GCOV_DIR)/gcc

CC_CXX_WARN_STRICT =
