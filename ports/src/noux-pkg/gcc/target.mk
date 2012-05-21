PROGRAM_PREFIX = genode-x86-

REQUIRES = x86_32

PWD = $(shell pwd)

NOUX_CFLAGS += -std=c99 -fgnu89-inline
NOUX_CONFIGURE_ARGS = --program-prefix=$(PROGRAM_PREFIX) \
                      --target=i686-freebsd \
                      --with-gnu-as --with-gnu-ld --disable-tls --disable-threads \
                      --disable-multilib --disable-shared

# prevent building libgomp (OpenMP) and libmudflap (pointer debugging facility)
NOUX_CONFIGURE_ARGS += enable_libmudflap=no enable_libgomp=no

# needed by gcc/config/freebsd-spec.h
NOUX_CFLAGS_FOR_BUILD += -DFBSD_MAJOR=6
NOUX_CFLAGS           += -DFBSD_MAJOR=6

# work-around for the following error while building libgcc
#
# libgcc2.h:172: error: unknown machine mode ‘libgcc_cmp_return’
# libgcc2.h:173: error: unknown machine mode ‘libgcc_shift_count’

NOUX_CFLAGS   += -D__libgcc_cmp_return__=word -D__libgcc_shift_count__=word -D__unwind_word__=word
NOUX_CXXFLAGS += -D__libgcc_cmp_return__=word -D__libgcc_shift_count__=word -D__unwind_word__=word

# define __SIZEOF_* macros as required by unwind-dw2.c:
#
# unwind.h:242:4: error: #error "__SIZEOF_LONG__ macro not defined"

NOUX_CFLAGS   += -D__SIZEOF_LONG__=4 -D__SIZEOF_POINTER__=4
NOUX_CXXFLAGS += -D__SIZEOF_LONG__=4 -D__SIZEOF_POINTER__=4

NOUX_ENV += CC_FOR_TARGET=$(CC) CXX_FOR_TARGET=$(CXX) GCC_FOR_TARGET=$(CC) \
            LD_FOR_TARGET=$(LD) AS_FOR_TARGET=$(AS) AR_FOR_TARGET=$(AR)
NOUX_ENV += CC_FOR_BUILD=gcc LD_FOR_BUILD=ld \
            CFLAGS_FOR_BUILD='$(NOUX_CFLAGS_FOR_BUILD)' \
            CPPFLAGS_FOR_BUILD='' LDFLAGS_FOR_BUILD=''

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

LIBS = gmp mpfr

NOUX_LDFLAGS += -L$(PWD)

dummy_libs: libgmp.a libmpfr.a libc.a

libgmp.a libmpfr.a libc.a:
	$(VERBOSE)$(AR) -rc $@

Makefile: dummy_libs

#
# XXX Merge the following with noux.mk
#
TARGET ?= $(lastword $(subst /, ,$(PRG_DIR)))

NOUX_PKG ?= $(TARGET)

LIBS  += cxx env libc libm libc_noux

NOUX_PKG_DIR = $(wildcard $(REP_DIR)/contrib/$(NOUX_PKG)-*)

#
# Detect missing preparation of noux package
#
ifeq ($(NOUX_PKG_DIR),)
REQUIRES += prepare_$(NOUX_PKG)
endif

#
# Verbose build
#
ifeq ($(VERBOSE),)
NOUX_MAKE_VERBOSE = V=1
NOUX_CONFIGURE_VERBOSE =

#
# Non-verbose build
#
else
NOUX_MAKE_VERBOSE = -s
NOUX_CONFIGURE_VERBOSE = --quiet

# filter for configure output of noux package
NOUX_CONFIGURE_OUTPUT_FILTER = > stdout.log 2> stderr.log ||\
	  (echo "Error: configure script of $(NOUX_PKG) failed" && \
	   cat stderr.log && false)

# filter for make output of noux package
NOUX_BUILD_OUTPUT_FILTER = 2>&1 | sed "s/^/      [$(NOUX_PKG)]  /"

endif

NOUX_CONFIGURE_ARGS += --host x86-freebsd --build $(shell $(NOUX_PKG_DIR)/config.guess)
NOUX_CONFIGURE_ARGS += --prefix $(PWD)/install
NOUX_CONFIGURE_ARGS += $(NOUX_CONFIGURE_VERBOSE)
NOUX_CONFIGURE_ARGS += --srcdir=$(NOUX_PKG_DIR)

NOUX_LDFLAGS += -nostdlib $(CXX_LINK_OPT) $(CC_MARCH) -Wl,-T$(LD_SCRIPT_DYN) \
                -Wl,--dynamic-linker=$(DYNAMIC_LINKER).lib.so \
                -Wl,--eh-frame-hdr

LIBGCC = $(shell $(CC) $(CC_MARCH) -print-libgcc-file-name)

NOUX_CPPFLAGS += -nostdinc $(INCLUDES)
NOUX_CPPFLAGS += -D_GNU_SOURCE=1
NOUX_CPPFLAGS += $(CC_MARCH)
NOUX_CFLAGS += -ffunction-sections $(CC_OLEVEL) -nostdlib $(NOUX_CPPFLAGS)
NOUX_CXXFLAGS += -ffunction-sections $(CC_OLEVEL) -nostdlib $(NOUX_CPPFLAGS)

#
# We have to specify 'LINK_ITEMS' twice to resolve inter-library dependencies.
# Unfortunately, the use of '--start-group' and '--end-group' does not suffice
# in all cases because 'libtool' strips those arguments from the 'LIBS' variable.
#
NOUX_LIBS += -Wl,--start-group $(sort $(LINK_ITEMS)) $(sort $(LINK_ITEMS)) $(LIBGCC) -Wl,--end-group

NOUX_ENV += CC='$(CC)' LD='$(LD)' AR='$(AR)' LIBS='$(NOUX_LIBS)' \
            LDFLAGS='$(NOUX_LDFLAGS)' CFLAGS='$(NOUX_CFLAGS)' \
            CPPFLAGS='$(NOUX_CPPFLAGS)' CXXFLAGS='$(NOUX_CXXFLAGS)'

#
# Re-configure the Makefile if the Genode build environment changes
#
Makefile reconfigure: $(MAKEFILE_LIST)

#
# Invoke configure script with the Genode environment
#
Makefile reconfigure: noux_env.sh
	@$(MSG_CONFIG)$(TARGET)
	$(VERBOSE)source noux_env.sh && $(NOUX_PKG_DIR)/configure $(NOUX_ENV) $(NOUX_CONFIGURE_ARGS) $(NOUX_CONFIGURE_OUTPUT_FILTER)

noux_env.sh:
	$(VERBOSE)rm -f $@
	$(VERBOSE)echo "export CC='$(CC)'" >> $@
	$(VERBOSE)echo "export AR='$(AR)'" >> $@
	$(VERBOSE)echo "export LD='$(LD)'" >> $@
	$(VERBOSE)echo "export CPPFLAGS='$(NOUX_CPPFLAGS)'" >> $@
	$(VERBOSE)echo "export CFLAGS='$(NOUX_CFLAGS)'" >> $@
	$(VERBOSE)echo "export CXXFLAGS='$(NOUX_CXXFLAGS)'" >> $@
	$(VERBOSE)echo "export LDFLAGS='$(NOUX_LDFLAGS)'" >> $@
	$(VERBOSE)echo "export LIBS='$(NOUX_LIBS)'" >> $@
	$(VERBOSE)echo "export CC_FOR_TARGET='$(CC)'" >> $@
	$(VERBOSE)echo "export CXX_FOR_TARGET='$(CXX)'" >> $@
	$(VERBOSE)echo "export GCC_FOR_TARGET='$(CC)'" >> $@
	$(VERBOSE)echo "export LD_FOR_TARGET='$(LD)'" >> $@
	$(VERBOSE)echo "export AS_FOR_TARGET='$(AS)'" >> $@
	$(VERBOSE)echo "export AR_FOR_TARGET='$(AR)'" >> $@
	$(VERBOSE)echo "export LDFLAGS_FOR_TARGET='$(NOUX_LDFLAGS)'" >> $@
	$(VERBOSE)echo "export LIBS_FOR_TARGET='$(NOUX_LIBS)'" >> $@
	$(VERBOSE)echo "export CFLAGS_FOR_BUILD='$(NOUX_CFLAGS_FOR_BUILD)'" >> $@
	$(VERBOSE)echo "export CPPFLAGS_FOR_BUILD=''" >> $@
	$(VERBOSE)echo "export LDFLAGS_FOR_BUILD=''" >> $@
	$(VERBOSE)echo "export LIBS_FOR_BUILD=''" >> $@
	$(VERBOSE)echo "export PS1='<noux>'" >> $@

#
# Invoke the 'Makefile' generated by the configure script
#
# The 'MAN=' argument prevent manual pages from being created. This would
# produce an error when the package uses the 'help2man' tool. This tool
# tries to extract the man page of a program by executing it with the
# '--help' argument on the host. However, we cannot run Genode binaries
# this way.
#
noux_built.tag: noux_env.sh Makefile
	@$(MSG_BUILD)$(TARGET)
	$(VERBOSE)source noux_env.sh && $(MAKE) $(NOUX_MAKE_ENV) $(NOUX_MAKE_VERBOSE) MAN= $(NOUX_BUILD_OUTPUT_FILTER)
	@touch $@

noux_installed.tag: noux_built.tag
	@$(MSG_INST)$(TARGET)
	$(VERBOSE)$(MAKE) $(NOUX_MAKE_VERBOSE) install MAN= >> stdout.log 2>> stderr.log
	$(VERBOSE)rm -f $(INSTALL_DIR)/$(TARGET)
	$(VERBOSE)ln -sf $(PWD)/install $(INSTALL_DIR)/$(TARGET)
	@touch $@

$(TARGET): noux_installed.tag

#
# The clean rule is expected to be executed within the 3rd-party build
# directory. The check should prevent serious damage if this condition
# is violated (e.g., while working on the build system).
#
ifeq ($(notdir $(PWD)),$(notdir $(PRG_DIR)))
clean_noux_build_dir:
	$(VERBOSE)rm -rf $(PWD)

clean_prg_objects: clean_noux_build_dir
endif

