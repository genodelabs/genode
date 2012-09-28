PWD = $(shell pwd)

NOUX_CONFIGURE_ARGS = --program-prefix=$(PROGRAM_PREFIX) \
                      --target=$(GCC_TARGET) \
                      --with-gnu-as --with-gnu-ld --disable-tls --disable-threads --disable-lto \
                      --disable-multilib

#
# Configure options passed to gcc
#
# The 't-linux' tmake file is needed to let the tool chain use 'unwind-dw2-fde-glibc',
# needed for the exception handling on Genode in the presence of shared libraries.
#
HOST_CONFIG_ARGS = host_xm_include_list=$(LINK_SPEC_H_$(PLATFORM)) \
                   tmake_file='t-slibgcc-elf-ver t-slibgcc-nolc-override t-linux'

# passed to target components such as libgcc, libstdc++
TARGET_CONFIG_ARGS = extra_parts='crtbegin.o crtbeginS.o crtbeginT.o crtend.o crtendS.o'

# compile libsupc++ as position-independent code
TARGET_CONFIG_ARGS += LIBSUPCXX_PICFLAGS='-prefer-pic'


NOUX_ENV += host_configargs="$(HOST_CONFIG_ARGS)" \
            target_configargs="$(TARGET_CONFIG_ARGS)"

NOUX_ENV += CC_FOR_TARGET=$(CC) CXX_FOR_TARGET=$(CXX) GCC_FOR_TARGET=$(CC) CPP_FOR_TARGET="$(CC) -E" \
            LD_FOR_TARGET=$(LD) AS_FOR_TARGET=$(AS) AR_FOR_TARGET=$(AR)

#
# Need to specify LDFLAGS_FOR_TARGET as configure argument, not just as
# environment variable. Otherwise, the generated Makefile will set 'LDFLAGS_FOR_TARGET'
# to empty, target libraries will fail to build.
#
NOUX_ENV += LDFLAGS_FOR_TARGET='$(NOUX_LDFLAGS)'
NOUX_ENV += CPPFLAGS_FOR_TARGET='$(NOUX_CPPFLAGS)'
NOUX_ENV += CXXFLAGS_FOR_TARGET='$(NOUX_CXXFLAGS)'

#
# We link libraries to the final binaries using the 'LIBS' variable.  But
# unfortunately, the gcc build system has hardcoded some libs such as '-lc'.
# To satisfy the linker, we provide dummy archives.
#

LIBS = gmp mpfr mpc

NOUX_LDFLAGS += -L$(PWD)

.SECONDARY: dummy_libs
dummy_libs: libgmp.a libmpfr.a libmpc.a libc.a

libgmp.a libmpfr.a libmpc.a libc.a:
	$(VERBOSE)$(AR) -rc $@

Makefile: dummy_libs

LIBS  += cxx env libc libm libc_noux

NOUX_PKG_DIR = $(wildcard $(REP_DIR)/contrib/gcc-*)

include $(REP_DIR)/mk/noux.mk

NOUX_CXXFLAGS += -ffunction-sections $(CC_OLEVEL) -nostdlib $(NOUX_CPPFLAGS)

#
# We have to specify 'LINK_ITEMS' twice to resolve inter-library dependencies.
# Unfortunately, the use of '--start-group' and '--end-group' does not suffice
# in all cases because 'libtool' strips those arguments from the 'LIBS' variable.
#
NOUX_LIBS += -Wl,--start-group $(sort $(LINK_ITEMS)) $(sort $(LINK_ITEMS)) $(LIBGCC) -Wl,--end-group
