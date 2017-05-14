PWD = $(shell pwd)

PKG_DIR = $(call select_from_ports,gcc)/src/noux-pkg/gcc

CONFIGURE_ARGS = --program-prefix=$(PROGRAM_PREFIX) \
                 --target=$(GCC_TARGET) \
                 --enable-languages=c,c++ \
                 --disable-libgo \
                 --disable-libssp \
                 --disable-libquadmath \
                 --disable-libffi \
                 --enable-targets=all \
                 --with-gnu-as \
                 --with-gnu-ld \
                 --disable-tls \
                 --disable-threads \
                 --disable-hosted-libstdcxx \
                 --enable-shared \
                 --enable-multiarch \
                 --disable-sjlj-exceptions

ENV += host_configargs="$(HOST_CONFIG_ARGS)" \
       target_configargs="$(TARGET_CONFIG_ARGS)"

ENV += CC_FOR_TARGET=$(CC) CXX_FOR_TARGET=$(CXX) GCC_FOR_TARGET=$(CC) CPP_FOR_TARGET="$(CC) -E" \
       LD_FOR_TARGET=$(LD) AS_FOR_TARGET=$(AS) AR_FOR_TARGET=$(AR)

# libgcc does not evaluate CPPFLAGS_FOR_TARGET, so everything is put into CFLAGS_FOR_TARGET here
ENV += CFLAGS_FOR_TARGET='-I$(BASE_DIR)/../../tool -DUSE_PT_GNU_EH_FRAME -Dinhibit_libc -fPIC'

# libsupc++
ENV += CXXFLAGS_FOR_TARGET='-fPIC'

MAKE_ENV += GENODE="yes"

#
# We link libraries to the final binaries using the 'LIBS' variable.  But
# unfortunately, the gcc build system has hardcoded some libs such as '-lc'.
# To satisfy the linker, we provide dummy archives.
#

LIBS = gmp mpfr mpc

LDFLAGS += -L$(PWD)

.SECONDARY: dummy_libs
dummy_libs: libgmp.a libmpfr.a libmpc.a libc.a

libgmp.a libmpfr.a libmpc.a libc.a:
	$(VERBOSE)$(AR) -rc $@

Makefile: dummy_libs

LIBS  += libc libm posix libc_noux stdcxx

include $(call select_from_repositories,mk/noux.mk)

#
# We have to specify 'LINK_ITEMS' twice to resolve inter-library dependencies.
# Unfortunately, the use of '--start-group' and '--end-group' does not suffice
# in all cases because 'libtool' strips those arguments from the 'LIBS' variable.
#
LDLIBS += -Wl,--start-group $(sort $(STATIC_LIBS)) $(sort $(STATIC_LIBS)) $(LIBGCC) -Wl,--end-group
