PKG_DIR := $(call select_from_ports,gdb)/src/noux-pkg/gdb

CONFIGURE_ARGS += --program-prefix=$(PROGRAM_PREFIX) \
                  --target=$(GDB_TARGET) \
                  --disable-sim

# the configure script calls the linker with "-lexpat", which fails
CONFIGURE_ARGS += --without-expat
CPPFLAGS += -DHAVE_LIBEXPAT=1

# prevent use of host libgmp in configure test
CONFIGURE_ARGS += --without-libgmp-prefix

CFLAGS += -Wno-error=sizeof-pointer-memaccess \
          -Wno-error=unused-value \
          -Wno-error=overflow

LIBS += stdcxx ncurses expat gmp

# workaround for problems with makeinfo version 5
MAKE_ENV += "MAKEINFO=true"

include $(call select_from_repositories,mk/noux.mk)
