CXX_SRC_CC += misc.cc new_delete.cc malloc_free.cc exception.cc guard.cc emutls.cc
INC_DIR += $(REP_DIR)/src/include
# We need the libsupc++ include directory
STDINC = yes

vpath %.cc $(BASE_DIR)/src/lib/cxx
vpath %.c  $(BASE_DIR)/src/lib/cxx

#
# Here we define all symbols we want to hide in libsupc++ and libgcc_eh
#
LIBC_SYMBOLS += malloc free calloc realloc \
                abort fputc fputs fwrite \
                stderr strcat strcpy strlen \
                memcmp strncmp strcmp sprintf \
                __stderrp

#
# Symbols we wrap (see unwind.cc)
#
EH_SYMBOLS = _Unwind_Resume _Unwind_Complete _Unwind_DeleteException

#
# Additional functions for ARM
#
EH_SYMBOLS += __aeabi_unwind_cpp_pr0 __aeabi_unwind_cpp_pr1

#
# Take the right system libraries
#
# Normally, we never include build-system-internal files from library-
# description files. For building the 'cxx' library, however, we need the
# information about the used 'gcc' for resolving the location of the C++
# support libraries. This definition is performed by 'mk/lib.mk' after
# including this library description file. Hence, we need to manually
# include 'global.mk' here.
#
include $(BASE_DIR)/mk/global.mk

LIBCXX_GCC = $(shell $(CUSTOM_CXX_LIB) $(CC_MARCH) -print-file-name=libsupc++.a) \
             $(shell $(CUSTOM_CXX_LIB) $(CC_MARCH) -print-file-name=libgcc_eh.a || true)

#
# Dummy target used by the build system
#
SRC_S         = supc++.o
SRC_C         = unwind.o
CXX_SRC       = $(sort $(CXX_SRC_CC))
CXX_OBJECTS   = $(addsuffix .o,$(basename $(CXX_SRC)))
LOCAL_SYMBOLS = $(patsubst %,--localize-symbol=%,$(LIBC_SYMBOLS))
REDEF_SYMBOLS = $(foreach S, $(EH_SYMBOLS), --redefine-sym $(S)=_cxx_$(S))

#
# Prevent symbols of the gcc support libs from being discarded during 'ld -r'
#
KEEP_SYMBOLS += __cxa_guard_acquire
KEEP_SYMBOLS += __dynamic_cast
KEEP_SYMBOLS += __cxa_throw_bad_array_new_length
KEEP_SYMBOLS += __cxa_current_exception_type
KEEP_SYMBOLS += _ZTVN10__cxxabiv116__enum_type_infoE
KEEP_SYMBOLS += _ZN10__cxxabiv121__vmi_class_type_infoD0Ev
KEEP_SYMBOLS += _ZTVN10__cxxabiv119__pointer_type_infoE
KEEP_SYMBOLS += _ZTSN10__cxxabiv120__function_type_infoE

#
# Include dependency files for the corresponding object files except
# when cleaning.
#
# Normally, the inclusion of dependency files is handled by 'generic.mk'.
# However, the mechanism in 'generic.mk' considers only the dependencies
# for the compilation units contained in the 'OBJECTS' variable. For building
# the cxx library, we rely on the 'CXX_OBJECTS' variable instead. So we need to
# include the dependenies manually.
#
ifneq ($(filter-out $(MAKECMDGOALS),clean),)
-include $(CXX_OBJECTS:.o=.d)
endif

#
# Rule to link all libc definitions and libsupc++ libraries
# and to hide after that the exported libc symbols
#
$(SRC_S): $(CXX_OBJECTS)
	$(MSG_MERGE)$@
	$(VERBOSE)$(LD) $(LD_MARCH) $(addprefix -u ,$(KEEP_SYMBOLS)) -r $(CXX_OBJECTS) $(LIBCXX_GCC) -o $@.tmp
	$(MSG_CONVERT)$@
	$(VERBOSE)$(OBJCOPY) $(LOCAL_SYMBOLS) $(REDEF_SYMBOLS) $@.tmp $@
	$(VERBOSE)$(RM) $@.tmp
