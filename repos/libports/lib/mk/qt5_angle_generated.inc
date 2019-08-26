QT_DEFINES += -DBUILDING_QT__=1 -DNDEBUG -DENABLE_3D_RENDERING=1 -DENABLE_ACCELERATED_2D_CANVAS=1 -DENABLE_BLOB=1 -DENABLE_CANVAS_PATH=1 -DENABLE_CHANNEL_MESSAGING=1 -DENABLE_CSS_BOX_DECORATION_BREAK=1 -DENABLE_CSS_COMPOSITING=1 -DENABLE_CSS_EXCLUSIONS=1 -DENABLE_CSS_FILTERS=1 -DENABLE_CSS_IMAGE_SET=1 -DENABLE_CSS_REGIONS=1 -DENABLE_CSS_SHAPES=1 -DENABLE_CSS_STICKY_POSITION=1 -DENABLE_CSS_TRANSFORMS_ANIMATIONS_UNPREFIXED=1 -DENABLE_DATALIST_ELEMENT=1 -DENABLE_DETAILS_ELEMENT=1 -DENABLE_DOWNLOAD_ATTRIBUTE=1 -DENABLE_FAST_MOBILE_SCROLLING=1 -DENABLE_FILTERS=1 -DENABLE_FTPDIR=1 -DENABLE_FULLSCREEN_API=1 -DENABLE_GESTURE_EVENTS=1 -DENABLE_ICONDATABASE=1 -DENABLE_IFRAME_SEAMLESS=1 -DENABLE_INPUT_TYPE_COLOR=1 -DENABLE_INSPECTOR=1 -DENABLE_INSPECTOR_SERVER=1 -DENABLE_JAVASCRIPT_DEBUGGER=1 -DENABLE_LEGACY_NOTIFICATIONS=1 -DENABLE_LEGACY_VIEWPORT_ADAPTION=1 -DENABLE_LEGACY_VENDOR_PREFIXES=1 -DENABLE_LEGACY_WEB_AUDIO=1 -DENABLE_LINK_PREFETCH=1 -DENABLE_METER_ELEMENT=1 -DENABLE_MHTML=1 -DENABLE_NOTIFICATIONS=1 -DENABLE_PAGE_VISIBILITY_API=1 -DENABLE_PROGRESS_ELEMENT=1 -DENABLE_RESOLUTION_MEDIA_QUERY=1 -DENABLE_REQUEST_ANIMATION_FRAME=1 -DENABLE_SHARED_WORKERS=1 -DENABLE_SMOOTH_SCROLLING=1 -DENABLE_SQL_DATABASE=1 -DENABLE_SUBPIXEL_LAYOUT=1 -DENABLE_SVG=1 -DENABLE_SVG_FONTS=1 -DENABLE_TOUCH_ADJUSTMENT=1 -DENABLE_TOUCH_EVENTS=1 -DENABLE_TOUCH_SLIDER=1 -DENABLE_VIEW_MODE_CSS_MEDIA=1 -DENABLE_WEB_SOCKETS=1 -DENABLE_WEB_TIMING=1 -DENABLE_WORKERS=1 -DENABLE_XHR_TIMEOUT=1 -DWTF_USE_TILED_BACKING_STORE=1 -DWTF_USE_CROSS_PLATFORM_CONTEXT_MENUS=1 -DHAVE_QTPRINTSUPPORT=1 -DHAVE_QSTYLE=1 -DHAVE_QTTESTLIB=1 -DWTF_USE_LIBJPEG=1 -DWTF_USE_LIBPNG=1 -DHAVE_FONTCONFIG=1 -DPLUGIN_ARCHITECTURE_UNSUPPORTED=1 -DWTF_USE_3D_GRAPHICS=1 -DENABLE_WEBGL=1 -DENABLE_BATTERY_STATUS=0 -DENABLE_CANVAS_PROXY=0 -DENABLE_CSP_NEXT=0 -DENABLE_CSS_GRID_LAYOUT=0 -DENABLE_CSS_HIERARCHIES=0 -DENABLE_CSS_IMAGE_ORIENTATION=0 -DENABLE_CSS_IMAGE_RESOLUTION=0 -DENABLE_CSS_SHADERS=0 -DENABLE_CSS_VARIABLES=0 -DENABLE_CSS3_CONDITIONAL_RULES=0 -DENABLE_CSS3_TEXT=0 -DENABLE_CSS3_TEXT_LINE_BREAK=0 -DENABLE_DASHBOARD_SUPPORT=0 -DENABLE_DATAGRID=0 -DENABLE_DATA_TRANSFER_ITEMS=0 -DENABLE_DEVICE_ORIENTATION=0 -DENABLE_DIRECTORY_UPLOAD=0 -DENABLE_FILE_SYSTEM=0 -DENABLE_FONT_LOAD_EVENTS=0 -DENABLE_GAMEPAD=0 -DENABLE_GEOLOCATION=0 -DENABLE_HIGH_DPI_CANVAS=0 -DENABLE_INDEXED_DATABASE=0 -DENABLE_INPUT_SPEECH=0 -DENABLE_INPUT_TYPE_DATE=0 -DENABLE_INPUT_TYPE_DATETIME_INCOMPLETE=0 -DENABLE_INPUT_TYPE_DATETIMELOCAL=0 -DENABLE_INPUT_TYPE_MONTH=0 -DENABLE_INPUT_TYPE_TIME=0 -DENABLE_INPUT_TYPE_WEEK=0 -DENABLE_LEGACY_CSS_VENDOR_PREFIXES=0 -DENABLE_MATHML=0 -DENABLE_MEDIA_SOURCE=0 -DENABLE_MEDIA_STATISTICS=0 -DENABLE_MEDIA_STREAM=0 -DENABLE_MICRODATA=0 -DENABLE_MOUSE_CURSOR_SCALE=0 -DENABLE_NAVIGATOR_CONTENT_UTILS=0 -DENABLE_NETSCAPE_PLUGIN_API=0 -DENABLE_NETWORK_INFO=0 -DENABLE_NOSNIFF=0 -DENABLE_ORIENTATION_EVENTS=0 -DENABLE_PROXIMITY_EVENTS=0 -DENABLE_QUOTA=0 -DENABLE_RESOURCE_TIMING=0 -DENABLE_SCRIPTED_SPEECH=0 -DENABLE_SECCOMP_FILTERS=0 -DENABLE_SHADOW_DOM=0 -DENABLE_STYLE_SCOPED=0 -DENABLE_TEMPLATE_ELEMENT=0 -DENABLE_TEXT_AUTOSIZING=0 -DENABLE_THREADED_HTML_PARSER=0 -DENABLE_TOUCH_ICON_LOADING=0 -DENABLE_USER_TIMING=0 -DENABLE_VIBRATION=0 -DENABLE_VIDEO=0 -DENABLE_VIDEO_TRACK=0 -DENABLE_WEB_AUDIO=0 -DENABLE_XSLT=0 -DBUILDING_ANGLE -DBUILDING_WEBKIT -DQT_ASCII_CAST_WARNINGS -DQT_NO_EXCEPTIONS -D_LARGEFILE64_SOURCE -D_LARGEFILE_SOURCE -DQT_NO_DEBUG

QT_INCPATH += \
  qtwebkit/Source/ThirdParty/ANGLE \
  qtwebkit/Source/ThirdParty/ANGLE/include/GLSLANG \
  qtwebkit/Source/ThirdParty/ANGLE/include/KHR \
  qtwebkit/Source/ThirdParty/ANGLE/src \
  qtwebkit/Source/ThirdParty/ANGLE/src/compiler/preprocessor \
  qtwebkit/Source/ThirdParty/ANGLE/include \
  qtwebkit/Source/ThirdParty/ANGLE/include \
  qtwebkit/Source/ThirdParty/ANGLE/include/GLSLANG \
  qtwebkit/Source/ThirdParty/ANGLE/include/KHR \
  qtwebkit/Source/ThirdParty/ANGLE/src \
  qtwebkit/Source/ThirdParty/ANGLE/src/compiler/preprocessor \
  qtwebkit/Source \
  qtbase/mkspecs/genode-g++

QT_SOURCES += \
  BuiltInFunctionEmulator.cpp \
  CodeGenGLSL.cpp \
  Compiler.cpp \
  debug.cpp \
  DependencyGraph.cpp \
  DependencyGraphBuilder.cpp \
  DependencyGraphOutput.cpp \
  DependencyGraphTraverse.cpp \
  DetectCallDepth.cpp \
  DetectDiscontinuity.cpp \
  Diagnostics.cpp \
  DirectiveHandler.cpp \
  ForLoopUnroll.cpp \
  InfoSink.cpp \
  Initialize.cpp \
  InitializeDll.cpp \
  InitializeParseContext.cpp \
  Intermediate.cpp \
  intermOut.cpp \
  IntermTraverse.cpp \
  MapLongVariableNames.cpp \
  OutputESSL.cpp \
  OutputGLSL.cpp \
  OutputGLSLBase.cpp \
  OutputHLSL.cpp \
  parseConst.cpp \
  ParseHelper.cpp \
  PoolAlloc.cpp \
  QualifierAlive.cpp \
  RemoveTree.cpp \
  SearchSymbol.cpp \
  ShaderLang.cpp \
  SymbolTable.cpp \
  RestrictFragmentShaderTiming.cpp \
  RestrictVertexShaderTiming.cpp \
  TranslatorESSL.cpp \
  TranslatorGLSL.cpp \
  TranslatorHLSL.cpp \
  Uniform.cpp \
  UnfoldShortCircuit.cpp \
  util.cpp \
  ValidateLimitations.cpp \
  VariableInfo.cpp \
  VariablePacker.cpp \
  VersionGLSL.cpp \
  DiagnosticsBase.cpp \
  DirectiveHandlerBase.cpp \
  DirectiveParser.cpp \
  Input.cpp \
  Lexer.cpp \
  Macro.cpp \
  MacroExpander.cpp \
  Preprocessor.cpp \
  Token.cpp \
  ArrayBoundsClamper.cpp \
  ossource_posix.cpp \
  glslang_lex.cpp \
  Tokenizer_lex.cpp \
  glslang_tab.cpp \
  ExpressionParser_tab.cpp 

QT_VPATH += \
  qtwebkit/Source/ThirdParty/ANGLE/src/compiler \
  qtwebkit/Source/ThirdParty/ANGLE/src/compiler/depgraph \
  qtwebkit/Source/ThirdParty/ANGLE/src/compiler/preprocessor \
  qtwebkit/Source/ThirdParty/ANGLE/src/compiler/timing \
  qtwebkit/Source/ThirdParty/ANGLE/src/third_party/compiler \

# some source files need to be generated by moc from other source/header files before
# they get #included again by the original source file in the compiling stage

# source files generated from existing header files ('moc_%.cpp: %.h' rule in import-qt5.inc)
# extracted from 'compiler_moc_header_make_all' target


# source files generated from existing source files ('%.moc: %.cpp' rule in import-qt5.inc)
# extracted from 'compiler_moc_source_make_all' rule


