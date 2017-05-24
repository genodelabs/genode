include $(REP_DIR)/lib/import/import-qt5_webcore.mk

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

#
# Generated files
#
# some of the following lines have been extracted from the console output
# of the 'configure' script (with modifications), that's why they can be
# quite long
#

ifneq ($(call select_from_ports,qt5),)
all: $(QT5_PORT_DIR)/src/lib/qt5/qtwebkit/Source/WebCore/generated/generated.tag
endif

# command names used by some of the extracted generator commands
DEL_FILE := rm
MOVE := mv

WEBCORE_DIR = $(QT5_CONTRIB_DIR)/qtwebkit/Source/WebCore

DEFINES = "LANGUAGE_JAVASCRIPT=1 ENABLE_3D_RENDERING=1 ENABLE_ACCELERATED_2D_CANVAS=1 ENABLE_BLOB=1 ENABLE_CANVAS_PATH=1 ENABLE_CHANNEL_MESSAGING=1 ENABLE_CSS_BOX_DECORATION_BREAK=1 ENABLE_CSS_COMPOSITING=1 ENABLE_CSS_EXCLUSIONS=1 ENABLE_CSS_FILTERS=1 ENABLE_CSS_IMAGE_SET=1 ENABLE_CSS_REGIONS=1 ENABLE_CSS_SHAPES=1 ENABLE_CSS_STICKY_POSITION=1 ENABLE_CSS_TRANSFORMS_ANIMATIONS_UNPREFIXED=1 ENABLE_DATALIST_ELEMENT=1 ENABLE_DETAILS_ELEMENT=1 ENABLE_DOWNLOAD_ATTRIBUTE=1 ENABLE_FAST_MOBILE_SCROLLING=1 ENABLE_FILTERS=1 ENABLE_FTPDIR=1 ENABLE_FULLSCREEN_API=1 ENABLE_GESTURE_EVENTS=1 ENABLE_ICONDATABASE=1 ENABLE_IFRAME_SEAMLESS=1 ENABLE_INPUT_TYPE_COLOR=1 ENABLE_INSPECTOR=1 ENABLE_INSPECTOR_SERVER=1 ENABLE_JAVASCRIPT_DEBUGGER=1 ENABLE_LEGACY_NOTIFICATIONS=1 ENABLE_LEGACY_VIEWPORT_ADAPTION=1 ENABLE_LEGACY_VENDOR_PREFIXES=1 ENABLE_LEGACY_WEB_AUDIO=1 ENABLE_LINK_PREFETCH=1 ENABLE_METER_ELEMENT=1 ENABLE_MHTML=1 ENABLE_NOTIFICATIONS=1 ENABLE_PAGE_VISIBILITY_API=1 ENABLE_PROGRESS_ELEMENT=1 ENABLE_RESOLUTION_MEDIA_QUERY=1 ENABLE_REQUEST_ANIMATION_FRAME=1 ENABLE_SHARED_WORKERS=1 ENABLE_SMOOTH_SCROLLING=1 ENABLE_SQL_DATABASE=1 ENABLE_SUBPIXEL_LAYOUT=1 ENABLE_SVG=1 ENABLE_SVG_FONTS=1 ENABLE_TOUCH_ADJUSTMENT=1 ENABLE_TOUCH_EVENTS=1 ENABLE_TOUCH_SLIDER=1 ENABLE_VIEW_MODE_CSS_MEDIA=1 ENABLE_WEB_SOCKETS=1 ENABLE_WEB_TIMING=1 ENABLE_WORKERS=1 ENABLE_XHR_TIMEOUT=1 ENABLE_WEBGL=1"
EXTRA_DEFINES = "QT_NO_MTDEV QT_NO_LIBUDEV QT_NO_TSLIB QT_NO_LIBINPUT ENABLE_3D_RENDERING=1 ENABLE_ACCELERATED_2D_CANVAS=1 ENABLE_BLOB=1 ENABLE_CANVAS_PATH=1 ENABLE_CHANNEL_MESSAGING=1 ENABLE_CSS_BOX_DECORATION_BREAK=1 ENABLE_CSS_COMPOSITING=1 ENABLE_CSS_EXCLUSIONS=1 ENABLE_CSS_FILTERS=1 ENABLE_CSS_IMAGE_SET=1 ENABLE_CSS_REGIONS=1 ENABLE_CSS_SHAPES=1 ENABLE_CSS_STICKY_POSITION=1 ENABLE_CSS_TRANSFORMS_ANIMATIONS_UNPREFIXED=1 ENABLE_DATALIST_ELEMENT=1 ENABLE_DETAILS_ELEMENT=1 ENABLE_DOWNLOAD_ATTRIBUTE=1 ENABLE_FAST_MOBILE_SCROLLING=1 ENABLE_FILTERS=1 ENABLE_FTPDIR=1 ENABLE_FULLSCREEN_API=1 ENABLE_GESTURE_EVENTS=1 ENABLE_ICONDATABASE=1 ENABLE_IFRAME_SEAMLESS=1 ENABLE_INPUT_TYPE_COLOR=1 ENABLE_INSPECTOR=1 ENABLE_INSPECTOR_SERVER=1 ENABLE_JAVASCRIPT_DEBUGGER=1 ENABLE_LEGACY_NOTIFICATIONS=1 ENABLE_LEGACY_VIEWPORT_ADAPTION=1 ENABLE_LEGACY_VENDOR_PREFIXES=1 ENABLE_LEGACY_WEB_AUDIO=1 ENABLE_LINK_PREFETCH=1 ENABLE_METER_ELEMENT=1 ENABLE_MHTML=1 ENABLE_NOTIFICATIONS=1 ENABLE_PAGE_VISIBILITY_API=1 ENABLE_PROGRESS_ELEMENT=1 ENABLE_RESOLUTION_MEDIA_QUERY=1 ENABLE_REQUEST_ANIMATION_FRAME=1 ENABLE_SHARED_WORKERS=1 ENABLE_SMOOTH_SCROLLING=1 ENABLE_SQL_DATABASE=1 ENABLE_SUBPIXEL_LAYOUT=1 ENABLE_SVG=1 ENABLE_SVG_FONTS=1 ENABLE_TOUCH_ADJUSTMENT=1 ENABLE_TOUCH_EVENTS=1 ENABLE_TOUCH_SLIDER=1 ENABLE_VIEW_MODE_CSS_MEDIA=1 ENABLE_WEB_SOCKETS=1 ENABLE_WEB_TIMING=1 ENABLE_WORKERS=1 ENABLE_XHR_TIMEOUT=1 WTF_USE_TILED_BACKING_STORE=1 WTF_USE_CROSS_PLATFORM_CONTEXT_MENUS=1 HAVE_QTQUICK=1 HAVE_QTPRINTSUPPORT=1 HAVE_QSTYLE=1 HAVE_QTTESTLIB=1 WTF_USE_LIBJPEG=1 WTF_USE_LIBPNG=1 PLUGIN_ARCHITECTURE_UNSUPPORTED=1 WTF_USE_3D_GRAPHICS=1 ENABLE_WEBGL=1 ENABLE_BATTERY_STATUS=0 ENABLE_CANVAS_PROXY=0 ENABLE_CSP_NEXT=0 ENABLE_CSS_GRID_LAYOUT=0 ENABLE_CSS_HIERARCHIES=0 ENABLE_CSS_IMAGE_ORIENTATION=0 ENABLE_CSS_IMAGE_RESOLUTION=0 ENABLE_CSS_SHADERS=0 ENABLE_CSS_VARIABLES=0 ENABLE_CSS3_CONDITIONAL_RULES=0 ENABLE_CSS3_TEXT=0 ENABLE_CSS3_TEXT_LINE_BREAK=0 ENABLE_DASHBOARD_SUPPORT=0 ENABLE_DATAGRID=0 ENABLE_DATA_TRANSFER_ITEMS=0 ENABLE_DEVICE_ORIENTATION=0 ENABLE_DIRECTORY_UPLOAD=0 ENABLE_FILE_SYSTEM=0 ENABLE_FONT_LOAD_EVENTS=0 ENABLE_GAMEPAD=0 ENABLE_GEOLOCATION=0 ENABLE_HIGH_DPI_CANVAS=0 ENABLE_INDEXED_DATABASE=0 ENABLE_INPUT_SPEECH=0 ENABLE_INPUT_TYPE_DATE=0 ENABLE_INPUT_TYPE_DATETIME_INCOMPLETE=0 ENABLE_INPUT_TYPE_DATETIMELOCAL=0 ENABLE_INPUT_TYPE_MONTH=0 ENABLE_INPUT_TYPE_TIME=0 ENABLE_INPUT_TYPE_WEEK=0 ENABLE_LEGACY_CSS_VENDOR_PREFIXES=0 ENABLE_MATHML=0 ENABLE_MEDIA_SOURCE=0 ENABLE_MEDIA_STATISTICS=0 ENABLE_MEDIA_STREAM=0 ENABLE_MICRODATA=0 ENABLE_MOUSE_CURSOR_SCALE=0 ENABLE_NAVIGATOR_CONTENT_UTILS=0 ENABLE_NETSCAPE_PLUGIN_API=0 ENABLE_NETWORK_INFO=0 ENABLE_NOSNIFF=0 ENABLE_ORIENTATION_EVENTS=0 ENABLE_PROXIMITY_EVENTS=0 ENABLE_QUOTA=0 ENABLE_RESOURCE_TIMING=0 ENABLE_SCRIPTED_SPEECH=0 ENABLE_SECCOMP_FILTERS=0 ENABLE_SHADOW_DOM=0 ENABLE_STYLE_SCOPED=0 ENABLE_TEMPLATE_ELEMENT=0 ENABLE_TEXT_AUTOSIZING=0 ENABLE_THREADED_HTML_PARSER=0 ENABLE_TOUCH_ICON_LOADING=0 ENABLE_USER_TIMING=0 ENABLE_VIBRATION=0 ENABLE_VIDEO=0 ENABLE_VIDEO_TRACK=0 ENABLE_WEB_AUDIO=0 ENABLE_XSLT=0"
GENERATE_BINDINGS_PL = export "SOURCE_ROOT=$(WEBCORE_DIR)" && perl -I$(WEBCORE_DIR)/bindings/scripts $(WEBCORE_DIR)/bindings/scripts/generate-bindings.pl --defines $(DEFINES) --generator JS --include Modules/filesystem --include Modules/geolocation --include Modules/indexeddb --include Modules/mediasource --include Modules/notifications --include Modules/quota --include Modules/webaudio --include Modules/webdatabase --include Modules/websockets --include css --include dom --include editing --include fileapi --include html --include html/canvas --include html/shadow --include html/track --include inspector --include loader/appcache --include page --include plugins --include storage --include svg --include testing --include workers --include xml --outputDir $(dir $@) --supplementalDependencyFile $(dir $@)/supplemental_dependency.tmp --idlAttributesFile $(WEBCORE_DIR)/bindings/scripts/IDLAttributes.txt --preprocessor "$(MOC) -E"
# The directory with the generated files must be added for the Genode build
# system, because it is not a subdirectory of the current directory.
GENERATE_BINDINGS_PL += --include $(dir $@)

# make the 'HOST_TOOLS' variable known
include $(REP_DIR)/lib/mk/qt5_host_tools.mk

$(QT5_PORT_DIR)/src/lib/qt5/qtwebkit/Source/WebCore/generated/generated.tag: $(HOST_TOOLS)

	$(VERBOSE)mkdir -p $(dir $@)

	@# make_names.pl
	$(VERBOSE)perl -I$(WEBCORE_DIR)/bindings/scripts $(WEBCORE_DIR)/dom/make_names.pl --tags $(WEBCORE_DIR)/mathml/mathtags.in --attrs $(WEBCORE_DIR)/mathml/mathattrs.in          --extraDefines $(EXTRA_DEFINES) --preprocessor "$(MOC) -E" --factory --wrapperFactory --outputDir $(dir $@)
	$(VERBOSE)perl -I$(WEBCORE_DIR)/bindings/scripts $(WEBCORE_DIR)/dom/make_names.pl --tags $(WEBCORE_DIR)/svg/svgtags.in --attrs $(WEBCORE_DIR)/svg/svgattrs.in                  --extraDefines $(EXTRA_DEFINES) --preprocessor "$(MOC) -E" --factory --wrapperFactory --outputDir $(dir $@)
	$(VERBOSE)perl -I$(WEBCORE_DIR)/bindings/scripts $(WEBCORE_DIR)/dom/make_names.pl --attrs $(WEBCORE_DIR)/svg/xlinkattrs.in --preprocessor "$(MOC) -E" --outputDir $(dir $@)

	@# makeprop.pl
	$(VERBOSE)perl -ne "print $1" $(WEBCORE_DIR)/css/CSSPropertyNames.in $(WEBCORE_DIR)/css/SVGCSSPropertyNames.in > $(dir $@)/CSSPropertyNames.in && cd $(dir $@) && perl -I$(WEBCORE_DIR)/bindings/scripts $(WEBCORE_DIR)/css/makeprop.pl --defines $(DEFINES) --preprocessor "$(MOC) -E" $(WEBCORE_DIR)/css/CSSPropertyNames.in && $(DEL_FILE) CSSPropertyNames.in CSSPropertyNames.gperf

	@# make_settings.pl
	$(VERBOSE)perl -I$(WEBCORE_DIR)/bindings/scripts $(WEBCORE_DIR)/page/make_settings.pl --input $(WEBCORE_DIR)/page/Settings.in --outputDir $(dir $@)

	@# makevalues.pl
	$(VERBOSE)perl -ne "print $1" $(WEBCORE_DIR)/css/CSSValueKeywords.in $(WEBCORE_DIR)/css/SVGCSSValueKeywords.in > $(dir $@)/CSSValueKeywords.in && cd $(dir $@) && perl -I$(WEBCORE_DIR)/bindings/scripts $(WEBCORE_DIR)/css/makevalues.pl --defines $(DEFINES) --preprocessor "$(MOC) -E" $(WEBCORE_DIR)/css/CSSValueKeywords.in && $(DEL_FILE) CSSValueKeywords.in CSSValueKeywords.gperf

	@# preprocess-idls.pl
	$(VERBOSE)sed -e "s,^qtwebkit,$(QT5_CONTRIB_DIR)/qtwebkit,g" -e "s,^generated/,$(dir $@),g" $(REP_DIR)/src/lib/qt5/qtwebkit/Source/WebCore/idl_files > $(dir $@)/idl_files.tmp
	$(VERBOSE)export "QT5_CONTRIB_DIR=$(QT5_CONTRIB_DIR)" && perl -I$(WEBCORE_DIR)/bindings/scripts $(WEBCORE_DIR)/bindings/scripts/preprocess-idls.pl --defines $(DEFINES) --idlFilesList $(dir $@)/idl_files.tmp --supplementalDependencyFile $(dir $@)/supplemental_dependency.tmp  --windowConstructorsFile $(dir $@)/DOMWindowConstructors.idl --workerGlobalScopeConstructorsFile $(dir $@)/WorkerGlobalScopeConstructors.idl --sharedWorkerGlobalScopeConstructorsFile $(dir $@)/SharedWorkerGlobalScopeConstructors.idl --dedicatedWorkerGlobalScopeConstructorsFile $(dir $@)/DedicatedWorkerGlobalScopeConstructors.idl

	@# generate-bindings.pl
	$(VERBOSE)while read -r idl_file; do $(GENERATE_BINDINGS_PL) $$idl_file; done < $(dir $@)/idl_files.tmp
	#$(VERBOSE)while read -r idl_file; do echo "$(GENERATE_BINDINGS_PL) $$idl_file"; $(GENERATE_BINDINGS_PL) $$idl_file; done < $(dir $@)/idl_files.tmp

	@# CodeGeneratorInspector.py
	$(VERBOSE)python $(WEBCORE_DIR)/inspector/CodeGeneratorInspector.py $(WEBCORE_DIR)/inspector/Inspector.json --output_h_dir $(dir $@) --output_cpp_dir $(dir $@)

	@# xxd.pl
	$(VERBOSE)perl $(WEBCORE_DIR)/inspector/xxd.pl InspectorOverlayPage_html $(WEBCORE_DIR)/inspector/InspectorOverlayPage.html $(dir $@)/InspectorOverlayPage.h
	$(VERBOSE)perl $(WEBCORE_DIR)/inspector/xxd.pl InjectedScriptSource_js $(WEBCORE_DIR)/inspector/InjectedScriptSource.js $(dir $@)/InjectedScriptSource.h
	$(VERBOSE)perl $(WEBCORE_DIR)/inspector/xxd.pl InjectedScriptCanvasModuleSource_js $(WEBCORE_DIR)/inspector/InjectedScriptCanvasModuleSource.js $(dir $@)/InjectedScriptCanvasModuleSource.h

	@# makegrammar.pl
	$(VERBOSE)perl -I $(WEBCORE_DIR)/bindings/scripts $(WEBCORE_DIR)/css/makegrammar.pl --outputDir $(dir $@) --extraDefines $(EXTRA_DEFINES) --preprocessor "$(MOC) -E" --symbolsPrefix cssyy $(WEBCORE_DIR)/css/CSSGrammar.y.in

	@# make_names.pl
	$(VERBOSE)perl -I$(WEBCORE_DIR)/bindings/scripts $(WEBCORE_DIR)/dom/make_names.pl --tags $(WEBCORE_DIR)/html/HTMLTagNames.in --attrs $(WEBCORE_DIR)/html/HTMLAttributeNames.in --extraDefines $(EXTRA_DEFINES) --preprocessor "$(MOC) -E" --factory --wrapperFactory --outputDir $(dir $@)
	$(VERBOSE)perl -I$(WEBCORE_DIR)/bindings/scripts $(WEBCORE_DIR)/dom/make_names.pl --attrs $(WEBCORE_DIR)/xml/xmlnsattrs.in --preprocessor "$(MOC) -E" --outputDir $(dir $@)
	$(VERBOSE)perl -I$(WEBCORE_DIR)/bindings/scripts $(WEBCORE_DIR)/dom/make_names.pl --attrs $(WEBCORE_DIR)/xml/xmlattrs.in --preprocessor "$(MOC) -E" --outputDir $(dir $@)
	$(VERBOSE)perl -I$(WEBCORE_DIR)/bindings/scripts $(WEBCORE_DIR)/dom/make_names.pl --fonts $(WEBCORE_DIR)/css/WebKitFontFamilyNames.in --outputDir $(dir $@)

	@# make_event_factory.pl
	$(VERBOSE)perl -I$(WEBCORE_DIR)/bindings/scripts $(WEBCORE_DIR)/dom/make_event_factory.pl --input $(WEBCORE_DIR)/dom/EventNames.in --outputDir $(dir $@)
	$(VERBOSE)perl -I$(WEBCORE_DIR)/bindings/scripts $(WEBCORE_DIR)/dom/make_event_factory.pl --input $(WEBCORE_DIR)/dom/EventTargetFactory.in --outputDir $(dir $@)

	@# make-dom-exceptions.pl
	$(VERBOSE)perl -I$(WEBCORE_DIR)/bindings/scripts $(WEBCORE_DIR)/dom/make_dom_exceptions.pl --input $(WEBCORE_DIR)/dom/DOMExceptions.in --outputDir $(dir $@)

	@# create-html-entity-table
	$(VERBOSE)python $(WEBCORE_DIR)/html/parser/create-html-entity-table -o $(dir $@)/HTMLEntityTable.cpp $(WEBCORE_DIR)/html/parser/HTMLEntityNames.in

	@# make-hash-tools.pl
	$(VERBOSE)perl $(WEBCORE_DIR)/make-hash-tools.pl $(dir $@) $(WEBCORE_DIR)/platform/ColorData.gperf

	@# make-css-file-arrays.pl
	$(VERBOSE)perl $(WEBCORE_DIR)/css/make-css-file-arrays.pl $(dir $@)/UserAgentStyleSheets.h $(dir $@)/UserAgentStyleSheetsData.cpp $(WEBCORE_DIR)/css/html.css $(WEBCORE_DIR)/css/quirks.css $(WEBCORE_DIR)/css/mathml.css $(WEBCORE_DIR)/css/svg.css $(WEBCORE_DIR)/css/view-source.css $(WEBCORE_DIR)/css/fullscreen.css $(WEBCORE_DIR)/css/mediaControls.css $(WEBCORE_DIR)/css/mediaControlsQt.css $(WEBCORE_DIR)/css/mediaControlsQtFullscreen.css $(WEBCORE_DIR)/css/plugIns.css $(WEBCORE_DIR)/css/themeQtNoListboxes.css $(WEBCORE_DIR)/css/mobileThemeQt.css
	$(VERBOSE)perl $(WEBCORE_DIR)/css/make-css-file-arrays.pl $(dir $@)/PlugInsResources.h $(dir $@)/PlugInsResourcesData.cpp $(WEBCORE_DIR)/Resources/plugIns.js

	@# XPathGrammar
	$(VERBOSE)bison -d -p xpathyy $(WEBCORE_DIR)/xml/XPathGrammar.y -o $(dir $@)/XPathGrammar.tab.c && $(MOVE) $(dir $@)/XPathGrammar.tab.c $(dir $@)/XPathGrammar.cpp && $(MOVE) $(dir $@)/XPathGrammar.tab.h $(dir $@)/XPathGrammar.h

	@# generate-webkit-version.pl
	$(VERBOSE)perl $(WEBCORE_DIR)/../WebKit/scripts/generate-webkitversion.pl --config $(WEBCORE_DIR)/../WebKit/mac/Configurations/Version.xcconfig --outputDir $(dir $@)/

	$(VERBOSE)touch $@

include $(REP_DIR)/lib/mk/qt5_webcore_generated.inc

QT_INCPATH += qtwebkit/Source/WebCore/generated

QT_VPATH += qtwebkit/Source/WebCore/generated

# InspectorBackendCommands.qrc, WebKit.qrc
QT_VPATH += qtwebkit/Source/WebCore/inspector/front-end

# WebCore.qrc
QT_VPATH += qtwebkit/Source/WebCore

include $(REP_DIR)/lib/mk/qt5.inc

LIBS += qt5_angle qt5_wtf qt5_jscore qt5_sql qt5_network qt5_gui qt5_core icu jpeg libpng zlib libc libm
