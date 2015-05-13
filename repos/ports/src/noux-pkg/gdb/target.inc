NOUX_PKG_DIR = $(call select_from_ports,gdb)/src/noux-pkg/gdb

NOUX_CONFIGURE_ARGS += --program-prefix=$(PROGRAM_PREFIX) \
                       --target=$(GDB_TARGET)

# the configure script calls the linker with "-lexpat", which fails
NOUX_CONFIGURE_ARGS += --without-expat
NOUX_CPPFLAGS += -DHAVE_LIBEXPAT=1
NOUX_CFLAGS += -Wno-error=sizeof-pointer-memaccess -Wno-error=unused-value

LIBS += ncurses expat

# workaround for problems with makeinfo version 5
NOUX_MAKE_ENV += "MAKEINFO=true"

NOUX_INSTALL_TARGET = install

include $(REP_DIR)/mk/noux.mk
