include $(call select_from_repositories,lib/import/import-qt5_webcore.mk)

SHARED_LIB = yes

# additional defines for the Genode version
CC_OPT += -DSQLITE_NO_SYNC=1 -DSQLITE_THREADSAFE=0

# enable C++ functions that use C99 math functions (disabled by default in the Genode tool chain)
CC_CXX_OPT += -D_GLIBCXX_USE_C99_MATH

# use default warning level to avoid noise when compiling contrib code
CC_WARN = -Wno-deprecated-declarations

CC_OPT_sqlite3 +=  -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast

# make sure that the correct "Comment.h" file gets included
QT_INCPATH := qtwebkit/Source/WebCore/dom

include $(REP_DIR)/lib/mk/qt5_webcore_generated.inc

QT_INCPATH += qtwebkit/Source/WebCore/generated

QT_VPATH += qtwebkit/Source/WebCore/generated

# InspectorBackendCommands.qrc, WebKit.qrc
QT_VPATH += qtwebkit/Source/WebCore/inspector/front-end

# WebCore.qrc
QT_VPATH += qtwebkit/Source/WebCore

include $(REP_DIR)/lib/mk/qt5.inc

LIBS += qt5_angle qt5_wtf qt5_jscore qt5_sql qt5_network qt5_gui qt5_core icu jpeg libpng zlib libc libm

CC_CXX_WARN_STRICT =
