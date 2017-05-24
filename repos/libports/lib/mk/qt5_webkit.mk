include $(REP_DIR)/lib/import/import-qt5_webkit.mk

SHARED_LIB = yes

# additional defines for the Genode version
CC_OPT += -DSQLITE_NO_SYNC=1 -DSQLITE_THREADSAFE=0

# enable C++ functions that use C99 math functions (disabled by default in the Genode tool chain)
CC_CXX_OPT += -D_GLIBCXX_USE_C99_MATH

# use default warning level to avoid noise when compiling contrib code
CC_WARN = -Wno-deprecated-declarations

CC_OPT_sqlite3 +=  -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast

include $(REP_DIR)/lib/mk/qt5_webkit_generated.inc

QT_INCPATH += qtwebkit/Source/WebCore/generated

include $(REP_DIR)/lib/mk/qt5.inc

LIBS += qt5_webcore qt5_jscore qt5_network qt5_printsupport qt5_gui qt5_core icu libc libm

vpath %.qrc $(QT5_CONTRIB_DIR)/src/3rdparty/webkit/Source/WebCore
vpath %.qrc $(QT5_CONTRIB_DIR)/src/3rdparty/webkit/Source/WebCore/inspector/front-end
