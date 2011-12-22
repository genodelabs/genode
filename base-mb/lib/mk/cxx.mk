LIBS        = allocator_avl 
CXX_SRC_CC += misc.cc new_delete.cc malloc_free.cc exception.cc guard.cc 

vpath %.cc $(BASE_DIR)/src/base/cxx

#
# Microblaze-specific supplement
#
CXX_SRC_CC += atexit.cc

vpath %.cc $(REP_DIR)/src/base/cxx

#
# Here we define all symbols we want to hide in libsupc++ and libgcc_eh
#
LIBC_SYMBOLS += malloc free calloc realloc \
                abort fputc fputs fwrite \
                stderr strcat strcpy strlen up \
                memcmp strncmp strcmp sprintf

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

LIBCXX_GCC = $(shell $(CUSTOM_CXX_LIB) -print-file-name=libsupc++.a) \
             $(shell $(CUSTOM_CXX_LIB) -print-libgcc-file-name)

# $(shell $(CUSTOM_CXX_LIB) -print-file-name=libgcc_eh.a)

#
# Dummy target used by the build system
#
SRC_S         = supc++.o
CXX_SRC       = $(sort $(CXX_SRC_CC))
CXX_OBJECTS   = $(addsuffix .o,$(basename $(CXX_SRC)))
LOCAL_SYMBOLS = $(patsubst %,--localize-symbol=%,$(LIBC_SYMBOLS))

#
# Prevent symbols of the gcc support libs from being discarded during 'ld -r'
#
KEEP_SYMBOLS += __cxa_guard_acquire
KEEP_SYMBOLS += __moddi3 __divdi3 __umoddi3 __udivdi3
KEEP_SYMBOLS += _ZTVN10__cxxabiv116__enum_type_infoE
KEEP_SYMBOLS += __fixunsdfdi
KEEP_SYMBOLS += __udivsi3 __divsi3

#
# Keep symbols additionally needed for linking the libc on ARM
#
KEEP_SYMBOLS += __muldi3 __eqdf2 __fixdfsi __ltdf2 __ltdf2 __nedf2 __ltdf2 \
                __gtdf2 __ltdf2 __ledf2 __fixdfsi __ltdf2 __ltdf2 __eqdf2 \
                __fixdfsi __ltdf2 __fixdfsi __eqdf2 __gtdf2 __ltdf2 __gtdf2 \
                __eqdf2 __muldi3 __muldi3

#
# Keep symbols needed for floating-point support on ARM
#
KEEP_SYMBOLS += __addsf3 __gtsf2 __ltsf2

#
# Additional symbols we need to keep when using the arm-none-linux-gnueabi
# tool chain
#
KEEP_SYMBOLS += __aeabi_ldivmod __aeabi_uldivmod __dynamic_cast
KEEP_SYMBOLS += _ZN10__cxxabiv121__vmi_class_type_infoD0Ev
KEEP_SYMBOLS += __aeabi_idiv __aeabi_ulcmp __aeabi_fmul __aeabi_dcmpun \
                __aeabi_d2lz __aeabi_f2lz  __aeabi_d2f  __aeabi_fcmpun \
                __aeabi_f2iz ctx_done sincos sincosf tgamma 

#
# Rule to link all libc definitions and libsupc++ libraries
# and to hide after that the exported libc symbols
#
$(SRC_S): $(CXX_OBJECTS)
	$(MSG_MERGE)$@
	$(VERBOSE)$(LD) $(addprefix -u ,$(KEEP_SYMBOLS)) -r $(CXX_OBJECTS) $(LIBCXX_GCC) -o $@.tmp
	$(MSG_CONVERT)$@
	$(VERBOSE)$(OBJCOPY) $(LOCAL_SYMBOLS) $@.tmp $@
	$(VERBOSE)$(RM) $@.tmp
