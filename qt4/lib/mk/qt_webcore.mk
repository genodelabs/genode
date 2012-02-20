include $(REP_DIR)/lib/import/import-qt_webcore.mk

SHARED_LIB = yes

# extracted from src/3rdparty/webkit/WebCore/Makefile
QT_DEFINES += -DBUILDING_QT__=1 -DWTF_USE_ACCELERATED_COMPOSITING -DNDEBUG -DQT_NO_CAST_TO_ASCII -DQT_ASCII_CAST_WARNINGS -DQT_MOC_COMPAT -DQT_USE_FAST_OPERATOR_PLUS -DQT_USE_FAST_CONCATENATION -DBUILD_WEBKIT -DENABLE_FAST_MOBILE_SCROLLING=1 -DBUILDING_QT__ -DBUILDING_JavaScriptCore -DBUILDING_WTF -DENABLE_VIDEO=0 -DENABLE_JAVASCRIPT_DEBUGGER=1 -DENABLE_DATABASE=1 -DENABLE_EVENTSOURCE=1 -DENABLE_OFFLINE_WEB_APPLICATIONS=1 -DENABLE_DOM_STORAGE=1 -DENABLE_ICONDATABASE=1 -DENABLE_CHANNEL_MESSAGING=1 -DENABLE_ORIENTATION_EVENTS=0 -DENABLE_SQLITE=1 -DENABLE_DASHBOARD_SUPPORT=0 -DENABLE_FILTERS=1 -DENABLE_XPATH=1 -DENABLE_WCSS=0 -DENABLE_WML=0 -DENABLE_SHARED_WORKERS=1 -DENABLE_WORKERS=1 -DENABLE_XHTMLMP=0 -DENABLE_DATAGRID=0 -DENABLE_RUBY=1 -DENABLE_SANDBOX=1 -DENABLE_PROGRESS_TAG=1 -DENABLE_BLOB_SLICE=0 -DENABLE_3D_RENDERING=1 -DENABLE_SVG=1 -DENABLE_SVG_FONTS=1 -DENABLE_SVG_FOREIGN_OBJECT=1 -DENABLE_SVG_ANIMATION=1 -DENABLE_SVG_AS_IMAGE=1 -DENABLE_SVG_USE=1 -DENABLE_DATALIST=1 -DENABLE_TILED_BACKING_STORE=1 -DENABLE_NETSCAPE_PLUGIN_API=0 -DENABLE_WEB_SOCKETS=1 -DENABLE_XSLT=0 -DENABLE_QT_BEARER=1 -DENABLE_TOUCH_EVENTS=1 -DSQLITE_CORE -DSQLITE_OMIT_LOAD_EXTENSION -DSQLITE_OMIT_COMPLETE -DQT_NO_DEBUG -DQT_GUI_LIB -DQT_NETWORK_LIB -DQT_CORE_LIB

# additional defines for the Genode version
CC_OPT += -DSQLITE_NO_SYNC=1 -DSQLITE_THREADSAFE=0

# enable C++ functions that use C99 math functions (disabled by default in the Genode tool chain)
CC_CXX_OPT += -D_GLIBCXX_USE_C99_MATH

# use default warning level to avoid noise when compiling contrib code
CC_WARN = -Wno-deprecated-declarations

CC_OPT_sqlite3 +=  -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast

include $(REP_DIR)/lib/mk/qt_webcore_generated.inc

INC_DIR += $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/platform/text \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/platform/text \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/JavaScriptCore \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/JavaScriptCore \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/JavaScriptCore/assembler \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/JavaScriptCore/assembler \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/JavaScriptCore/interpreter \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/JavaScriptCore/interpreter \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/JavaScriptCore/runtime \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/JavaScriptCore/runtime \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/JavaScriptCore/jit \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/JavaScriptCore/jit \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/JavaScriptCore/bytecode \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/JavaScriptCore/bytecode \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/JavaScriptCore/wtf \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/JavaScriptCore/wtf \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/JavaScriptCore/wtf/unicode \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/JavaScriptCore/wtf/unicode \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/JavaScriptCore/parser \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/JavaScriptCore/parser \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/JavaScriptCore/debugger \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/JavaScriptCore/debugger \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/JavaScriptCore/wrec \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/JavaScriptCore/wrec \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/JavaScriptCore/generated \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/JavaScriptCore/generated \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/JavaScriptCore/bytecompiler \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/JavaScriptCore/bytecompiler \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/JavaScriptCore/profiler \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/JavaScriptCore/profiler \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/JavaScriptCore/ForwardingHeaders \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/JavaScriptCore/ForwardingHeaders \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/JavaScriptCore/API \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/JavaScriptCore/API \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/JavaScriptCore/pcre \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/JavaScriptCore/pcre \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/editing \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/editing \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/dom \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/dom \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/platform \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/platform \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/html \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/html \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/html/canvas \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/html/canvas \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/platform/graphics \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/platform/graphics \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/platform/graphics/transforms \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/platform/graphics/transforms \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/css \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/css \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/generated \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/generated \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/rendering \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/rendering \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/loader \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/loader \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/platform/animation \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/platform/animation \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/platform/network \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/platform/network \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/platform/network/qt \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/platform/network/qt \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/rendering/style \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/rendering/style \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/svg \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/svg \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/svg/animation \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/svg/animation \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/svg/graphics \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/svg/graphics \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/svg/graphics/filters \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/svg/graphics/filters \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/history \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/history \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/page \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/page \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/page/animation \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/page/animation \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/platform/image-decoders \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/platform/image-decoders \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/platform/qt \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/platform/qt \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/platform/sql \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/platform/sql \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/platform/graphics/filters \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/platform/graphics/filters \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/platform/graphics/qt \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/platform/graphics/qt \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/platform/mock \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/platform/mock \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebKit/qt/Api/ \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebKit/qt/Api \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/bindings/js \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/bindings/js \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/inspector \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/inspector \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/bridge \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/bridge \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/bridge/c \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/bridge/c \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/bridge/jsc \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/bridge/jsc \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/bridge/qt \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/bridge/qt \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/storage \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/storage \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/plugins \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/plugins \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebKit/qt/WebCoreSupport/ \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebKit/qt/WebCoreSupport/ \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/loader/appcache \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/loader/appcache \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/loader/archive \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/loader/archive \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/loader/icon \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/loader/icon \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/xml \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/xml \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/xml \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/xml \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/workers \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/workers \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/notifications \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/notifications \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/accessibility \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/accessibility \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/websockets \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/websockets \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/sqlite \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/sqlite \
           $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/ \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/ \
           $(REP_DIR)/src/lib/qt4/src/corelib/global

LIBS += qt_jscore qt_network qt_core libc libm

vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/generated
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/bindings
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/bindings/js
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/bridge
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/bridge/c
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/bridge/jsc
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/bridge/qt
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/css
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/dom
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/dom/default
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/editing
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/editing/qt
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/history
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/history/qt
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/html
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/html/canvas
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/inspector
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/inspector/front-end
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/loader/appcache
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/loader/archive
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/loader
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/loader/icon
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/page
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/page/animation
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/page/qt
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/plugins
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/plugins/qt
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/platform
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/platform/qt
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/platform/animation
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/platform/text
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/platform/text/qt
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/platform/graphics
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/platform/graphics/filters
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/platform/graphics/qt
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/platform/graphics/transforms
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/platform/image-decoders/qt
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/platform/mock
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/platform/network
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/platform/network/qt
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/platform/sql
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/rendering
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/rendering/style
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/xml
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebKit/qt/WebCoreSupport
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebKit/qt/Api
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/sqlite
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/storage
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/svg
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/svg/animation
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/svg/graphics
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/svg/graphics/filters
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/svg/graphics/qt
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/workers
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/notifications
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/accessibility
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/accessibility/qt
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/webkit/WebCore/websockets

vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/generated
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/bindings
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/bindings/js
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/bridge
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/bridge/c
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/bridge/jsc
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/bridge/qt
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/css
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/dom
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/dom/default
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/editing
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/editing/qt
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/history
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/history/qt
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/html
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/html/canvas
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/inspector
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/inspector/front-end
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/loader/appcache
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/loader/archive
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/loader
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/loader/icon
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/page
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/page/animation
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/page/qt
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/plugins
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/plugins/qt
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/platform
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/platform/qt
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/platform/animation
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/platform/text
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/platform/text/qt
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/platform/graphics
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/platform/graphics/filters
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/platform/graphics/qt
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/platform/graphics/transforms
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/platform/image-decoders/qt
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/platform/mock
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/platform/network
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/platform/network/qt
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/platform/sql
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/rendering
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/rendering/style
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/xml
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebKit/qt/WebCoreSupport
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebKit/qt/Api
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/sqlite
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/storage
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/svg
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/svg/animation
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/svg/graphics
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/svg/graphics/filters
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/svg/graphics/qt
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/workers
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/notifications
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/accessibility
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/accessibility/qt
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/webkit/WebCore/websockets

include $(REP_DIR)/lib/mk/qt.inc
