include $(REP_DIR)/lib/import/import-qt_jscore.mk

SHARED_LIB = yes

# extracted from src/3rdparty/webkit/JavaScriptCore/Makefile
QT_DEFINES += -DBUILDING_QT__=1 -DWTF_USE_ACCELERATED_COMPOSITING -DNDEBUG -DBUILDING_QT__ -DBUILDING_JavaScriptCore -DBUILDING_WTF -DQT_NO_DEBUG -DQT_NETWORK_LIB -DQT_CORE_LIB

# additional defines for the Genode version
CC_OPT += -DSQLITE_NO_SYNC=1 -DSQLITE_THREADSAFE=0

# enable C++ functions that use C99 math functions (disabled by default in the Genode tool chain)
CC_CXX_OPT += -D_GLIBCXX_USE_C99_MATH

# use default warning level to avoid noise when compiling contrib code
CC_WARN =

include $(REP_DIR)/lib/mk/qt_jscore_generated.inc

INC_DIR += $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/JavaScriptCore \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/JavaScriptCore \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/JavaScriptCore/assembler \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/JavaScriptCore/assembler \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/JavaScriptCore/bytecode \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/JavaScriptCore/bytecode \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/JavaScriptCore/bytecompiler \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/JavaScriptCore/bytecompiler \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/JavaScriptCore/debugger \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/JavaScriptCore/debugger \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/JavaScriptCore/interpreter \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/JavaScriptCore/interpreter \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/JavaScriptCore/jit \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/JavaScriptCore/jit \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/JavaScriptCore/parser \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/JavaScriptCore/parser \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/JavaScriptCore/pcre \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/JavaScriptCore/pcre \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/JavaScriptCore/profiler \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/JavaScriptCore/profiler \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/JavaScriptCore/runtime \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/JavaScriptCore/runtime \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/JavaScriptCore/wtf \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/JavaScriptCore/wtf \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/JavaScriptCore/wtf/symbian \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/JavaScriptCore/wtf/symbian \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/JavaScriptCore/wtf/unicode \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/JavaScriptCore/wtf/unicode \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/JavaScriptCore/yarr \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/JavaScriptCore/yarr \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/JavaScriptCore/API \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/JavaScriptCore/API \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/JavaScriptCore/ForwardingHeaders \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/JavaScriptCore/ForwardingHeaders \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/JavaScriptCore/generated \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/JavaScriptCore/generated

LIBS += qt_network qt_core libc libm


vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/JavaScriptCore/pcre
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/JavaScriptCore/API
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/JavaScriptCore/assembler
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/JavaScriptCore/bytecode
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/JavaScriptCore/bytecompiler
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/JavaScriptCore/debugger
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/JavaScriptCore/interpreter
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/JavaScriptCore/jit
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/JavaScriptCore/parser
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/JavaScriptCore/profiler
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/JavaScriptCore/runtime
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/JavaScriptCore/wtf
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/JavaScriptCore/wtf/qt
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/JavaScriptCore/wtf/symbian
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/JavaScriptCore/wtf/unicode
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/JavaScriptCore/wtf/unicode/icu
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/JavaScriptCore/yarr
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/JavaScriptCore/generated


vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/JavaScriptCore/pcre
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/JavaScriptCore/API
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/JavaScriptCore/assembler
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/JavaScriptCore/bytecode
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/JavaScriptCore/bytecompiler
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/JavaScriptCore/debugger
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/JavaScriptCore/interpreter
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/JavaScriptCore/jit
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/JavaScriptCore/parser
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/JavaScriptCore/profiler
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/JavaScriptCore/runtime
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/JavaScriptCore/wtf
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/JavaScriptCore/wtf/qt
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/JavaScriptCore/wtf/symbian
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/JavaScriptCore/wtf/unicode
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/JavaScriptCore/wtf/unicode/icu
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/JavaScriptCore/yarr
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/JavaScriptCore/generated

include $(REP_DIR)/lib/mk/qt.inc
