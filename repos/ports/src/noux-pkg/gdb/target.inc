PKG_DIR = $(call select_from_ports,gdb)/src/noux-pkg/gdb

CONFIGURE_ARGS += --program-prefix=$(PROGRAM_PREFIX) \
                  --target=$(GDB_TARGET)

# the configure script calls the linker with "-lexpat", which fails
CONFIGURE_ARGS += --without-expat
CPPFLAGS += -DHAVE_LIBEXPAT=1
CFLAGS += -Wno-error=sizeof-pointer-memaccess -Wno-error=unused-value

LIBS += ncurses expat

# workaround for problems with makeinfo version 5
MAKE_ENV += "MAKEINFO=true"

INSTALL_TARGET = install

include $(REP_DIR)/mk/noux.mk
