include $(call select_from_repositories,lib/import/import-qt5_jscore.mk)

SHARED_LIB = yes

# additional defines for the Genode version
CC_OPT += -DSQLITE_NO_SYNC=1 -DSQLITE_THREADSAFE=0

# enable C++ functions that use C99 math functions (disabled by default in the Genode tool chain)
CC_CXX_OPT += -D_GLIBCXX_USE_C99_MATH

# use default warning level to avoid noise when compiling contrib code
CC_WARN =

include $(REP_DIR)/lib/mk/qt5_jscore_generated.inc

QT_INCPATH += qtwebkit/Source/JavaScriptCore/generated

include $(REP_DIR)/lib/mk/qt5.inc

LIBS += qt5_network qt5_core icu libc libm

CC_CXX_WARN_STRICT =
