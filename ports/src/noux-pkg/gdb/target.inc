NOUX_CONFIGURE_ARGS += --program-prefix=$(PROGRAM_PREFIX) \
                       --target=$(GDB_TARGET)

# the configure script calls the linker with "-lexpat", which fails
NOUX_CONFIGURE_ARGS += --without-expat
NOUX_CFLAGS += -DHAVE_LIBEXPAT=1

LIBS += ncurses expat

NOUX_PKG_DIR = $(wildcard $(REP_DIR)/contrib/gdb-*)

include $(REP_DIR)/mk/noux.mk
