#
# \brief  Download and prepare Qt4 source code
# \author Christian Prochaska
# \author Norman Feske
# \date   2009-05-11
#

REP_DIR := $(realpath .)
include $(REP_DIR)/lib/mk/qt5_version.inc

QT5_URL = http://download.qt-project.org/official_releases/qt/5.1/$(QT_VERSION)/single
QT5_TGZ = $(QT5).tar.gz
QT5_MD5 = 787ce18c7f47fc14538b4362a0aa9edd

QTSCRIPTCLASSIC_URL = http://ftp.heanet.ie/mirrors/ftp.trolltech.com/pub/qt/solutions/lgpl
QTSCRIPTCLASSIC     = qtscriptclassic-1.0_1-opensource
QTSCRIPTCLASSIC_TGZ = $(QTSCRIPTCLASSIC).tar.gz
QTSCRIPTCLASSIC_MD5 = a835edfa9de2206ebfaebcb1267453bf

#
# Interface to top-level prepare Makefile
#
PORTS += qt5

prepare-qt5: $(CONTRIB_DIR)/$(QT5) \
             $(CONTRIB_DIR)/$(QTSCRIPTCLASSIC) \
             tools \
             $(REP_DIR)/src/lib/qt5/qtwebkit/Source/JavaScriptCore/generated/generated.tag \
             $(REP_DIR)/src/lib/qt5/qtwebkit/Source/WebCore/generated/generated.tag

#
# Port-specific local rules
#
PATCHES_DIR  = src/lib/qt5/patches
PATCHES = $(shell cat $(PATCHES_DIR)/series)

$(call check_tool,wget)
$(call check_tool,patch)
$(call check_tool,bison)
$(call check_tool,perl)
$(call check_tool,python)
$(call check_tool,sed)
$(call check_tool,gperf)

$(DOWNLOAD_DIR)/$(QT5_TGZ):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(QT5_URL)/$(QT5_TGZ) && touch $@

$(DOWNLOAD_DIR)/$(QT5_TGZ).verified: $(DOWNLOAD_DIR)/$(QT5_TGZ)
	$(VERBOSE)$(HASHVERIFIER) $(DOWNLOAD_DIR)/$(QT5_TGZ) $(QT5_MD5) md5
	$(VERBOSE)touch $@

$(DOWNLOAD_DIR)/$(QTSCRIPTCLASSIC_TGZ): $(DOWNLOAD_DIR)
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(QTSCRIPTCLASSIC_URL)/$(QTSCRIPTCLASSIC_TGZ) && touch $@

$(DOWNLOAD_DIR)/$(QTSCRIPTCLASSIC_TGZ).verified: $(DOWNLOAD_DIR)/$(QTSCRIPTCLASSIC_TGZ)
	$(VERBOSE)$(HASHVERIFIER) $(DOWNLOAD_DIR)/$(QTSCRIPTCLASSIC_TGZ) $(QTSCRIPTCLASSIC_MD5) md5
	$(VERBOSE)touch $@

$(CONTRIB_DIR)/$(QT5): $(DOWNLOAD_DIR)/$(QT5_TGZ).verified
	$(VERBOSE)tar xzf $(DOWNLOAD_DIR)/$(QT5_TGZ) -C $(CONTRIB_DIR)
	$(VERBOSE)touch $(CONTRIB_DIR)/$(QT5)
	$(VERBOSE)git init $(CONTRIB_DIR)/$(QT5)
	$(VERBOSE)cd $(CONTRIB_DIR)/$(QT5) && for p in $(PATCHES); do \
	            git apply ../../$(PATCHES_DIR)/$$p; done;

$(CONTRIB_DIR)/$(QTSCRIPTCLASSIC): $(DOWNLOAD_DIR)/$(QTSCRIPTCLASSIC_TGZ).verified
	$(VERBOSE)tar xzf $(DOWNLOAD_DIR)/$(QTSCRIPTCLASSIC_TGZ) -C $(CONTRIB_DIR)
	$(VERBOSE)touch $(CONTRIB_DIR)/$(QTSCRIPTCLASSIC)
	$(VERBOSE)patch -d $(CONTRIB_DIR)/$(QTSCRIPTCLASSIC) -p1 -i ../../$(PATCHES_DIR)/qtscriptclassic_qt5.patch

#
# generated files
#
# some of the following lines have been extracted from Makefiles (and modified afterwards), that's why they can be quite long   
#

JAVASCRIPTCORE_DIR = $(CONTRIB_DIR)/$(QT5)/qtwebkit/Source/JavaScriptCore

$(REP_DIR)/src/lib/qt5/qtwebkit/Source/JavaScriptCore/generated/generated.tag:

	$(VERBOSE)mkdir -p $(dir $@)

	@# create_hash_table
	$(VERBOSE)perl $(JAVASCRIPTCORE_DIR)/create_hash_table $(JAVASCRIPTCORE_DIR)/runtime/ArrayConstructor.cpp -i  > $(dir $@)/ArrayConstructor.lut.h
	$(VERBOSE)perl $(JAVASCRIPTCORE_DIR)/create_hash_table $(JAVASCRIPTCORE_DIR)/runtime/ArrayPrototype.cpp -i    > $(dir $@)/ArrayPrototype.lut.h
	$(VERBOSE)perl $(JAVASCRIPTCORE_DIR)/create_hash_table $(JAVASCRIPTCORE_DIR)/runtime/BooleanPrototype.cpp -i  > $(dir $@)/BooleanPrototype.lut.h
	$(VERBOSE)perl $(JAVASCRIPTCORE_DIR)/create_hash_table $(JAVASCRIPTCORE_DIR)/runtime/DateConstructor.cpp -i   > $(dir $@)/DateConstructor.lut.h
	$(VERBOSE)perl $(JAVASCRIPTCORE_DIR)/create_hash_table $(JAVASCRIPTCORE_DIR)/runtime/DatePrototype.cpp -i     > $(dir $@)/DatePrototype.lut.h
	$(VERBOSE)perl $(JAVASCRIPTCORE_DIR)/create_hash_table $(JAVASCRIPTCORE_DIR)/runtime/ErrorPrototype.cpp -i    > $(dir $@)/ErrorPrototype.lut.h
	$(VERBOSE)perl $(JAVASCRIPTCORE_DIR)/create_hash_table $(JAVASCRIPTCORE_DIR)/runtime/JSGlobalObject.cpp -i    > $(dir $@)/JSGlobalObject.lut.h
	$(VERBOSE)perl $(JAVASCRIPTCORE_DIR)/create_hash_table $(JAVASCRIPTCORE_DIR)/runtime/JSONObject.cpp -i        > $(dir $@)/JSONObject.lut.h
	$(VERBOSE)perl $(JAVASCRIPTCORE_DIR)/create_hash_table $(JAVASCRIPTCORE_DIR)/runtime/MathObject.cpp -i        > $(dir $@)/MathObject.lut.h
	$(VERBOSE)perl $(JAVASCRIPTCORE_DIR)/create_hash_table $(JAVASCRIPTCORE_DIR)/runtime/NamePrototype.cpp -i     > $(dir $@)/NamePrototype.lut.h
	$(VERBOSE)perl $(JAVASCRIPTCORE_DIR)/create_hash_table $(JAVASCRIPTCORE_DIR)/runtime/NumberConstructor.cpp -i > $(dir $@)/NumberConstructor.lut.h
	$(VERBOSE)perl $(JAVASCRIPTCORE_DIR)/create_hash_table $(JAVASCRIPTCORE_DIR)/runtime/NumberPrototype.cpp -i   > $(dir $@)/NumberPrototype.lut.h
	$(VERBOSE)perl $(JAVASCRIPTCORE_DIR)/create_hash_table $(JAVASCRIPTCORE_DIR)/runtime/ObjectConstructor.cpp -i > $(dir $@)/ObjectConstructor.lut.h
	$(VERBOSE)perl $(JAVASCRIPTCORE_DIR)/create_hash_table $(JAVASCRIPTCORE_DIR)/runtime/ObjectPrototype.cpp -i   > $(dir $@)/ObjectPrototype.lut.h
	$(VERBOSE)perl $(JAVASCRIPTCORE_DIR)/create_hash_table $(JAVASCRIPTCORE_DIR)/runtime/RegExpConstructor.cpp -i > $(dir $@)/RegExpConstructor.lut.h
	$(VERBOSE)perl $(JAVASCRIPTCORE_DIR)/create_hash_table $(JAVASCRIPTCORE_DIR)/runtime/RegExpObject.cpp -i      > $(dir $@)/RegExpObject.lut.h
	$(VERBOSE)perl $(JAVASCRIPTCORE_DIR)/create_hash_table $(JAVASCRIPTCORE_DIR)/runtime/RegExpPrototype.cpp -i   > $(dir $@)/RegExpPrototype.lut.h
	$(VERBOSE)perl $(JAVASCRIPTCORE_DIR)/create_hash_table $(JAVASCRIPTCORE_DIR)/runtime/StringConstructor.cpp -i > $(dir $@)/StringConstructor.lut.h
	$(VERBOSE)perl $(JAVASCRIPTCORE_DIR)/create_hash_table $(JAVASCRIPTCORE_DIR)/runtime/StringPrototype.cpp -i   > $(dir $@)/StringPrototype.lut.h
	$(VERBOSE)perl $(JAVASCRIPTCORE_DIR)/create_hash_table $(JAVASCRIPTCORE_DIR)/parser/Keywords.table -i         > $(dir $@)/Lexer.lut.h

	@# KeywordLookupGenerator.py
	$(VERBOSE)python $(JAVASCRIPTCORE_DIR)/KeywordLookupGenerator.py $(JAVASCRIPTCORE_DIR)/parser/Keywords.table  > $(dir $@)/KeywordLookup.h

	@# create_regex_tables 
	$(VERBOSE)python $(JAVASCRIPTCORE_DIR)/create_regex_tables > $(dir $@)/RegExpJitTables.h

	$(VERBOSE)touch $@


# command names used by some of the extracted generator commands
DEL_FILE := rm
MOVE := mv

QT_DEFINES = "LANGUAGE_JAVASCRIPT=1 ENABLE_3D_RENDERING=1 ENABLE_BLOB=1 ENABLE_CHANNEL_MESSAGING=1 ENABLE_CSS_BOX_DECORATION_BREAK=1 ENABLE_CSS_COMPOSITING=1 ENABLE_CSS_EXCLUSIONS=1 ENABLE_CSS_FILTERS=1 ENABLE_CSS_IMAGE_SET=1 ENABLE_CSS_REGIONS=1 ENABLE_CSS_STICKY_POSITION=1 ENABLE_DATALIST_ELEMENT=1 ENABLE_DETAILS_ELEMENT=1 ENABLE_FAST_MOBILE_SCROLLING=1 ENABLE_FILTERS=1 ENABLE_FTPDIR=1 ENABLE_GESTURE_EVENTS=1 ENABLE_ICONDATABASE=1 ENABLE_IFRAME_SEAMLESS=1 ENABLE_INPUT_TYPE_COLOR=1 ENABLE_INSPECTOR=1 ENABLE_INSPECTOR_SERVER=1 ENABLE_JAVASCRIPT_DEBUGGER=1 ENABLE_LEGACY_NOTIFICATIONS=1 ENABLE_LEGACY_VIEWPORT_ADAPTION=1 ENABLE_LEGACY_VENDOR_PREFIXES=1 ENABLE_LINK_PREFETCH=1 ENABLE_METER_ELEMENT=1 ENABLE_MHTML=1 ENABLE_MUTATION_OBSERVERS=1 ENABLE_NOTIFICATIONS=1 ENABLE_PAGE_VISIBILITY_API=1 ENABLE_PROGRESS_ELEMENT=1 ENABLE_RESOLUTION_MEDIA_QUERY=1 ENABLE_REQUEST_ANIMATION_FRAME=1 ENABLE_SHARED_WORKERS=1 ENABLE_SMOOTH_SCROLLING=1 ENABLE_SQL_DATABASE=1 ENABLE_SVG=1 ENABLE_SVG_FONTS=1 ENABLE_TOUCH_ADJUSTMENT=1 ENABLE_TOUCH_EVENTS=1 ENABLE_WEB_SOCKETS=1 ENABLE_WEB_TIMING=1 ENABLE_WORKERS=1 ENABLE_XHR_TIMEOUT=1 ENABLE_TOUCH_SLIDER=1 ENABLE_ACCELERATED_2D_CANVAS=0 ENABLE_ANIMATION_API=0 ENABLE_BATTERY_STATUS=0 ENABLE_CSP_NEXT=0 ENABLE_CSS_GRID_LAYOUT=0 ENABLE_CSS_HIERARCHIES=0 ENABLE_CSS_IMAGE_ORIENTATION=0 ENABLE_CSS_IMAGE_RESOLUTION=0 ENABLE_CSS_SHADERS=0 ENABLE_CSS_VARIABLES=0 ENABLE_CSS3_BACKGROUND=0 ENABLE_CSS3_CONDITIONAL_RULES=0 ENABLE_CSS3_TEXT=0 ENABLE_DASHBOARD_SUPPORT=0 ENABLE_DATAGRID=0 ENABLE_DATA_TRANSFER_ITEMS=0 ENABLE_DEVICE_ORIENTATION=0 ENABLE_DIRECTORY_UPLOAD=0 ENABLE_DOWNLOAD_ATTRIBUTE=0 ENABLE_FILE_SYSTEM=0 ENABLE_FULLSCREEN_API=0 ENABLE_GAMEPAD=0 ENABLE_GEOLOCATION=0 ENABLE_HIGH_DPI_CANVAS=0 ENABLE_INDEXED_DATABASE=0 ENABLE_INPUT_SPEECH=0 ENABLE_INPUT_TYPE_DATE=0 ENABLE_INPUT_TYPE_DATETIME=0 ENABLE_INPUT_TYPE_DATETIMELOCAL=0 ENABLE_INPUT_TYPE_MONTH=0 ENABLE_INPUT_TYPE_TIME=0 ENABLE_INPUT_TYPE_WEEK=0 ENABLE_LEGACY_CSS_VENDOR_PREFIXES=0 ENABLE_LINK_PRERENDER=0 ENABLE_MATHML=0 ENABLE_MEDIA_SOURCE=0 ENABLE_MEDIA_STATISTICS=0 ENABLE_MEDIA_STREAM=0 ENABLE_MICRODATA=0 ENABLE_NAVIGATOR_CONTENT_UTILS=0 ENABLE_NETSCAPE_PLUGIN_API=0 ENABLE_NETWORK_INFO=0 ENABLE_ORIENTATION_EVENTS=0 ENABLE_PROXIMITY_EVENTS=0 ENABLE_QUOTA=0 ENABLE_SCRIPTED_SPEECH=0 ENABLE_SHADOW_DOM=0 ENABLE_STYLE_SCOPED=0 ENABLE_SVG_DOM_OBJC_BINDINGS=0 ENABLE_TEXT_AUTOSIZING=0 ENABLE_TEXT_NOTIFICATIONS_ONLY=0 ENABLE_TOUCH_ICON_LOADING=0 ENABLE_VIBRATION=0 ENABLE_VIDEO=0 ENABLE_VIDEO_TRACK=0 ENABLE_WEBGL=0 ENABLE_WEB_AUDIO=0 ENABLE_XSLT=0"
QT_EXTRA_DEFINES = "QT_NO_LIBUDEV QT_NO_XCB QT_NO_XKBCOMMON ENABLE_3D_RENDERING=1 ENABLE_BLOB=1 ENABLE_CHANNEL_MESSAGING=1 ENABLE_CSS_BOX_DECORATION_BREAK=1 ENABLE_CSS_COMPOSITING=1 ENABLE_CSS_EXCLUSIONS=1 ENABLE_CSS_FILTERS=1 ENABLE_CSS_IMAGE_SET=1 ENABLE_CSS_REGIONS=1 ENABLE_CSS_STICKY_POSITION=1 ENABLE_DATALIST_ELEMENT=1 ENABLE_DETAILS_ELEMENT=1 ENABLE_FAST_MOBILE_SCROLLING=1 ENABLE_FILTERS=1 ENABLE_FTPDIR=1 ENABLE_GESTURE_EVENTS=1 ENABLE_ICONDATABASE=1 ENABLE_IFRAME_SEAMLESS=1 ENABLE_INPUT_TYPE_COLOR=1 ENABLE_INSPECTOR=1 ENABLE_INSPECTOR_SERVER=1 ENABLE_JAVASCRIPT_DEBUGGER=1 ENABLE_LEGACY_NOTIFICATIONS=1 ENABLE_LEGACY_VIEWPORT_ADAPTION=1 ENABLE_LEGACY_VENDOR_PREFIXES=1 ENABLE_LINK_PREFETCH=1 ENABLE_METER_ELEMENT=1 ENABLE_MHTML=1 ENABLE_MUTATION_OBSERVERS=1 ENABLE_NOTIFICATIONS=1 ENABLE_PAGE_VISIBILITY_API=1 ENABLE_PROGRESS_ELEMENT=1 ENABLE_RESOLUTION_MEDIA_QUERY=1 ENABLE_REQUEST_ANIMATION_FRAME=1 ENABLE_SHARED_WORKERS=1 ENABLE_SMOOTH_SCROLLING=1 ENABLE_SQL_DATABASE=1 ENABLE_SVG=1 ENABLE_SVG_FONTS=1 ENABLE_TOUCH_ADJUSTMENT=1 ENABLE_TOUCH_EVENTS=1 ENABLE_WEB_SOCKETS=1 ENABLE_WEB_TIMING=1 ENABLE_WORKERS=1 ENABLE_XHR_TIMEOUT=1 WTF_USE_TILED_BACKING_STORE=1 HAVE_QTPRINTSUPPORT=1 HAVE_QSTYLE=1 HAVE_QTTESTLIB=1 WTF_USE_LIBJPEG=1 WTF_USE_LIBPNG=1 PLUGIN_ARCHITECTURE_UNSUPPORTED=1 ENABLE_TOUCH_SLIDER=1 ENABLE_ACCELERATED_2D_CANVAS=0 ENABLE_ANIMATION_API=0 ENABLE_BATTERY_STATUS=0 ENABLE_CSP_NEXT=0 ENABLE_CSS_GRID_LAYOUT=0 ENABLE_CSS_HIERARCHIES=0 ENABLE_CSS_IMAGE_ORIENTATION=0 ENABLE_CSS_IMAGE_RESOLUTION=0 ENABLE_CSS_SHADERS=0 ENABLE_CSS_VARIABLES=0 ENABLE_CSS3_BACKGROUND=0 ENABLE_CSS3_CONDITIONAL_RULES=0 ENABLE_CSS3_TEXT=0 ENABLE_DASHBOARD_SUPPORT=0 ENABLE_DATAGRID=0 ENABLE_DATA_TRANSFER_ITEMS=0 ENABLE_DEVICE_ORIENTATION=0 ENABLE_DIRECTORY_UPLOAD=0 ENABLE_DOWNLOAD_ATTRIBUTE=0 ENABLE_FILE_SYSTEM=0 ENABLE_FULLSCREEN_API=0 ENABLE_GAMEPAD=0 ENABLE_GEOLOCATION=0 ENABLE_HIGH_DPI_CANVAS=0 ENABLE_INDEXED_DATABASE=0 ENABLE_INPUT_SPEECH=0 ENABLE_INPUT_TYPE_DATE=0 ENABLE_INPUT_TYPE_DATETIME=0 ENABLE_INPUT_TYPE_DATETIMELOCAL=0 ENABLE_INPUT_TYPE_MONTH=0 ENABLE_INPUT_TYPE_TIME=0 ENABLE_INPUT_TYPE_WEEK=0 ENABLE_LEGACY_CSS_VENDOR_PREFIXES=0 ENABLE_LINK_PRERENDER=0 ENABLE_MATHML=0 ENABLE_MEDIA_SOURCE=0 ENABLE_MEDIA_STATISTICS=0 ENABLE_MEDIA_STREAM=0 ENABLE_MICRODATA=0 ENABLE_NAVIGATOR_CONTENT_UTILS=0 ENABLE_NETSCAPE_PLUGIN_API=0 ENABLE_NETWORK_INFO=0 ENABLE_ORIENTATION_EVENTS=0 ENABLE_PROXIMITY_EVENTS=0 ENABLE_QUOTA=0 ENABLE_SCRIPTED_SPEECH=0 ENABLE_SHADOW_DOM=0 ENABLE_STYLE_SCOPED=0 ENABLE_SVG_DOM_OBJC_BINDINGS=0 ENABLE_TEXT_AUTOSIZING=0 ENABLE_TEXT_NOTIFICATIONS_ONLY=0 ENABLE_TOUCH_ICON_LOADING=0 ENABLE_VIBRATION=0 ENABLE_VIDEO=0 ENABLE_VIDEO_TRACK=0 ENABLE_WEBGL=0 ENABLE_WEB_AUDIO=0 ENABLE_XSLT=0" 
GENERATE_BINDINGS_PL = $(VERBOSE)export "SOURCE_ROOT=$(WEBCORE_DIR)" && perl -I$(WEBCORE_DIR)/bindings/scripts $(WEBCORE_DIR)/bindings/scripts/generate-bindings.pl --defines $(QT_DEFINES) --generator JS --include Modules/filesystem --include Modules/geolocation --include Modules/indexeddb --include Modules/mediasource --include Modules/notifications --include Modules/quota --include Modules/webaudio --include Modules/webdatabase --include Modules/websockets --include css --include dom --include editing --include fileapi --include html --include html/canvas --include html/shadow --include html/track --include inspector --include loader/appcache --include page --include plugins --include storage --include svg --include testing --include workers --include xml --outputDir $(dir $@) --supplementalDependencyFile $(dir $@)/supplemental_dependency.tmp --preprocessor "$(REP_DIR)/tool/qt5/moc/moc -E"
# the absolute path is needed for makeprop.pl
WEBCORE_DIR = $(REP_DIR)/$(CONTRIB_DIR)/$(QT5)/qtwebkit/Source/WebCore
 
$(REP_DIR)/src/lib/qt5/qtwebkit/Source/WebCore/generated/generated.tag:

	$(VERBOSE)mkdir -p $(dir $@)

	$(VERBOSE)bison -d -p xpathyy $(WEBCORE_DIR)/xml/XPathGrammar.y -o $(dir $@)/XPathGrammar.tab.c && $(MOVE) $(dir $@)/XPathGrammar.tab.c $(dir $@)/XPathGrammar.cpp && $(MOVE) $(dir $@)/XPathGrammar.tab.h $(dir $@)/XPathGrammar.h

	@# preprocess-idls.pl
	$(VERBOSE)sed -e "s,^,$(CONTRIB_DIR)/$(QT5)/,g" $(dir $@)/../idl_files > $(dir $@)/idl_files.tmp
	$(VERBOSE)touch $(dir $@)/supplemental_dependency.tmp 
	$(VERBOSE)export "CONTRIB_DIR=$(CONTRIB_DIR)" && export "QT5=$(QT5)" && perl -I$(WEBCORE_DIR)/bindings/scripts $(WEBCORE_DIR)/bindings/scripts/preprocess-idls.pl --defines $(QT_DEFINES) --idlFilesList $(dir $@)/idl_files.tmp --supplementalDependencyFile $(dir $@)/supplemental_dependency.tmp --idlAttributesFile $(WEBCORE_DIR)/bindings/scripts/IDLAttributes.txt --preprocessor "$(REP_DIR)/tool/qt5/moc/moc -E"

	@# generate-bindings.pl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/filesystem/DOMFileSystem.idl 
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/filesystem/DOMFileSystemSync.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/filesystem/DOMWindowFileSystem.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/filesystem/DirectoryEntry.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/filesystem/DirectoryEntrySync.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/filesystem/DirectoryReader.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/filesystem/DirectoryReaderSync.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/filesystem/EntriesCallback.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/filesystem/Entry.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/filesystem/EntryArray.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/filesystem/EntryArraySync.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/filesystem/EntryCallback.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/filesystem/EntrySync.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/filesystem/ErrorCallback.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/filesystem/FileCallback.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/filesystem/FileEntry.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/filesystem/FileEntrySync.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/filesystem/FileSystemCallback.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/filesystem/FileWriter.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/filesystem/FileWriterCallback.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/filesystem/Metadata.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/filesystem/MetadataCallback.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/filesystem/WorkerContextFileSystem.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/geolocation/Geolocation.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/geolocation/Geoposition.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/geolocation/NavigatorGeolocation.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/geolocation/PositionCallback.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/geolocation/PositionError.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/geolocation/PositionErrorCallback.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/indexeddb/DOMWindowIndexedDatabase.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/indexeddb/IDBAny.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/indexeddb/IDBCursor.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/indexeddb/IDBDatabaseException.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/indexeddb/IDBDatabase.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/indexeddb/IDBFactory.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/indexeddb/IDBIndex.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/indexeddb/IDBKey.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/indexeddb/IDBKeyRange.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/indexeddb/IDBObjectStore.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/indexeddb/IDBRequest.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/indexeddb/IDBTransaction.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/indexeddb/WorkerContextIndexedDatabase.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/notifications/DOMWindowNotifications.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/notifications/Notification.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/notifications/NotificationCenter.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/notifications/NotificationPermissionCallback.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/notifications/WorkerContextNotifications.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/quota/DOMWindowQuota.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/quota/StorageInfo.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/quota/StorageInfoErrorCallback.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/quota/StorageInfoQuotaCallback.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/quota/StorageInfoUsageCallback.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/webaudio/AudioBuffer.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/webaudio/AudioBufferCallback.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/webaudio/AudioBufferSourceNode.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/webaudio/ChannelMergerNode.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/webaudio/ChannelSplitterNode.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/webaudio/AudioContext.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/webaudio/AudioDestinationNode.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/webaudio/AudioGain.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/webaudio/GainNode.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/webaudio/AudioListener.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/webaudio/AudioNode.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/webaudio/PannerNode.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/webaudio/AudioParam.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/webaudio/AudioProcessingEvent.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/webaudio/AudioSourceNode.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/webaudio/BiquadFilterNode.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/webaudio/ConvolverNode.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/webaudio/DelayNode.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/webaudio/DOMWindowWebAudio.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/webaudio/DynamicsCompressorNode.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/webaudio/ScriptProcessorNode.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/webaudio/MediaElementAudioSourceNode.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/webaudio/MediaStreamAudioSourceNode.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/webaudio/OfflineAudioCompletionEvent.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/webaudio/OscillatorNode.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/webaudio/AnalyserNode.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/webaudio/WaveShaperNode.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/webaudio/WaveTable.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/webdatabase/DOMWindowWebDatabase.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/webdatabase/Database.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/webdatabase/DatabaseCallback.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/webdatabase/DatabaseSync.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/webdatabase/SQLError.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/webdatabase/SQLException.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/webdatabase/SQLResultSet.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/webdatabase/SQLResultSetRowList.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/webdatabase/SQLStatementCallback.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/webdatabase/SQLStatementErrorCallback.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/webdatabase/SQLTransaction.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/webdatabase/SQLTransactionCallback.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/webdatabase/SQLTransactionErrorCallback.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/webdatabase/SQLTransactionSync.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/webdatabase/SQLTransactionSyncCallback.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/webdatabase/WorkerContextWebDatabase.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/websockets/CloseEvent.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/websockets/DOMWindowWebSocket.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/websockets/WebSocket.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/Modules/websockets/WorkerContextWebSocket.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/css/Counter.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/css/CSSCharsetRule.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/css/CSSFontFaceRule.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/css/CSSImportRule.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/css/CSSMediaRule.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/css/CSSPageRule.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/css/CSSPrimitiveValue.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/css/CSSRule.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/css/CSSRuleList.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/css/CSSStyleDeclaration.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/css/CSSStyleRule.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/css/CSSStyleSheet.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/css/CSSValue.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/css/CSSValueList.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/css/MediaList.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/css/MediaQueryList.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/css/Rect.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/css/RGBColor.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/css/StyleMedia.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/css/StyleSheet.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/css/StyleSheetList.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/css/WebKitCSSFilterValue.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/css/WebKitCSSKeyframeRule.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/css/WebKitCSSKeyframesRule.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/css/WebKitCSSMatrix.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/css/WebKitCSSMixFunctionValue.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/css/WebKitCSSRegionRule.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/css/WebKitCSSTransformValue.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/css/WebKitCSSViewportRule.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/Attr.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/BeforeLoadEvent.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/CharacterData.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/ClientRect.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/ClientRectList.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/Clipboard.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/CDATASection.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/Comment.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/CompositionEvent.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/CustomEvent.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/DataTransferItem.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/DataTransferItemList.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/DeviceMotionEvent.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/DeviceOrientationEvent.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/DocumentFragment.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/Document.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/DocumentType.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/DOMCoreException.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/DOMError.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/DOMImplementation.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/DOMStringList.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/DOMStringMap.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/Element.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/Entity.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/EntityReference.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/ErrorEvent.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/Event.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/EventException.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/EventTarget.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/HashChangeEvent.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/KeyboardEvent.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/MouseEvent.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/MessageChannel.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/MessageEvent.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/MessagePort.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/MutationCallback.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/MutationEvent.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/MutationObserver.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/MutationRecord.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/NamedNodeMap.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/Node.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/NodeFilter.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/NodeIterator.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/NodeList.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/Notation.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/OverflowEvent.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/PageTransitionEvent.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/PopStateEvent.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/ProcessingInstruction.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/ProgressEvent.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/PropertyNodeList.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/RangeException.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/Range.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/RequestAnimationFrameCallback.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/ShadowRoot.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/StringCallback.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/Text.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/TextEvent.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/Touch.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/TouchEvent.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/TouchList.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/TreeWalker.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/UIEvent.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/WebKitAnimationEvent.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/WebKitNamedFlow.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/DOMNamedFlowCollection.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/WebKitTransitionEvent.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/dom/WheelEvent.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/fileapi/Blob.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/fileapi/File.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/fileapi/FileError.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/fileapi/FileException.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/fileapi/FileList.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/fileapi/FileReader.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/fileapi/FileReaderSync.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/canvas/ArrayBufferView.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/canvas/ArrayBuffer.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/canvas/DataView.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/canvas/Int8Array.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/canvas/Float32Array.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/canvas/Float64Array.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/canvas/CanvasGradient.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/canvas/Int32Array.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/canvas/CanvasPattern.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/canvas/CanvasRenderingContext.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/canvas/CanvasRenderingContext2D.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/canvas/EXTTextureFilterAnisotropic.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/canvas/OESStandardDerivatives.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/canvas/OESTextureFloat.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/canvas/OESVertexArrayObject.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/canvas/OESElementIndexUint.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/canvas/WebGLActiveInfo.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/canvas/WebGLBuffer.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/canvas/WebGLCompressedTextureS3TC.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/canvas/WebGLContextAttributes.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/canvas/WebGLContextEvent.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/canvas/WebGLDebugRendererInfo.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/canvas/WebGLDebugShaders.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/canvas/WebGLDepthTexture.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/canvas/WebGLFramebuffer.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/canvas/WebGLLoseContext.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/canvas/WebGLProgram.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/canvas/WebGLRenderbuffer.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/canvas/WebGLRenderingContext.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/canvas/WebGLShader.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/canvas/WebGLShaderPrecisionFormat.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/canvas/Int16Array.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/canvas/WebGLTexture.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/canvas/WebGLUniformLocation.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/canvas/WebGLVertexArrayObjectOES.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/canvas/Uint8Array.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/canvas/Uint8ClampedArray.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/canvas/Uint32Array.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/canvas/Uint16Array.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/DOMFormData.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/DOMSettableTokenList.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/DOMTokenList.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/DOMURL.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLAllCollection.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLAudioElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLAnchorElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLAppletElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLAreaElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLBaseElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLBaseFontElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLBodyElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLBRElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLButtonElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLCanvasElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLCollection.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLDataListElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLDetailsElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLDialogElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLDirectoryElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLDivElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLDListElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLDocument.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLEmbedElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLFieldSetElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLFontElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLFormControlsCollection.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLFormElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLFrameElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLFrameSetElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLHeadElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLHeadingElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLHRElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLHtmlElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLIFrameElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLImageElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLInputElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLKeygenElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLLabelElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLLegendElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLLIElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLLinkElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLMapElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLMarqueeElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLMediaElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLMenuElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLMetaElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLMeterElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLModElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLObjectElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLOListElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLOptGroupElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLOptionElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLOptionsCollection.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLOutputElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLParagraphElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLParamElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLPreElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLProgressElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLPropertiesCollection.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLQuoteElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLScriptElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLSelectElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLSourceElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLSpanElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLStyleElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLTableCaptionElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLTableCellElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLTableColElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLTableElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLTableRowElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLTableSectionElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLTextAreaElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLTitleElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLTrackElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLUListElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLUnknownElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/HTMLVideoElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/ImageData.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/MediaController.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/MediaError.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/MicroDataItemValue.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/RadioNodeList.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/TextMetrics.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/TimeRanges.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/ValidityState.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/VoidCallback.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/shadow/HTMLContentElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/html/shadow/HTMLShadowElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/inspector/InjectedScriptHost.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/inspector/InspectorFrontendHost.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/inspector/JavaScriptCallFrame.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/inspector/ScriptProfile.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/inspector/ScriptProfileNode.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/loader/appcache/DOMApplicationCache.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/page/BarInfo.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/page/Console.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/page/Coordinates.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/page/Crypto.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/page/DOMSecurityPolicy.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/page/DOMSelection.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/page/DOMWindow.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/page/EventSource.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/page/History.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/page/Location.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/page/MemoryInfo.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/page/Navigator.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/page/Performance.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/page/PerformanceEntry.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/page/PerformanceEntryList.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/page/PerformanceNavigation.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/page/PerformanceResourceTiming.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/page/PerformanceTiming.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/page/Screen.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/page/SpeechInputEvent.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/page/SpeechInputResult.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/page/SpeechInputResultList.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/page/WebKitAnimation.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/page/WebKitAnimationList.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/page/WebKitPoint.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/page/WorkerNavigator.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/plugins/DOMPlugin.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/plugins/DOMMimeType.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/plugins/DOMPluginArray.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/plugins/DOMMimeTypeArray.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/storage/Storage.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/storage/StorageEvent.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/testing/Internals.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/testing/InternalSettings.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/testing/MallocStatistics.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/workers/AbstractWorker.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/workers/DedicatedWorkerContext.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/workers/SharedWorker.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/workers/SharedWorkerContext.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/workers/Worker.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/workers/WorkerContext.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/workers/WorkerLocation.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/xml/DOMParser.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/xml/XMLHttpRequest.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/xml/XMLHttpRequestException.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/xml/XMLHttpRequestProgressEvent.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/xml/XMLHttpRequestUpload.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/xml/XMLSerializer.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/xml/XPathNSResolver.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/xml/XPathException.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/xml/XPathExpression.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/xml/XPathResult.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/xml/XPathEvaluator.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/xml/XSLTProcessor.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGAElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGAltGlyphDefElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGAltGlyphElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGAltGlyphItemElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGAngle.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGAnimateColorElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGAnimateMotionElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGAnimatedAngle.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGAnimatedBoolean.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGAnimatedEnumeration.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGAnimatedInteger.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGAnimatedLength.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGAnimatedLengthList.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGAnimatedNumber.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGAnimatedNumberList.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGAnimatedPreserveAspectRatio.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGAnimatedRect.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGAnimatedString.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGAnimatedTransformList.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGAnimateElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGAnimateTransformElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGAnimationElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGCircleElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGClipPathElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGColor.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGComponentTransferFunctionElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGCursorElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGDefsElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGDescElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGDocument.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGElementInstance.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGElementInstanceList.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGEllipseElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGException.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGFEBlendElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGFEColorMatrixElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGFEComponentTransferElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGFECompositeElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGFEConvolveMatrixElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGFEDiffuseLightingElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGFEDisplacementMapElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGFEDistantLightElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGFEDropShadowElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGFEFloodElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGFEFuncAElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGFEFuncBElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGFEFuncGElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGFEFuncRElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGFEGaussianBlurElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGFEImageElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGFEMergeElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGFEMergeNodeElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGFEMorphologyElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGFEOffsetElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGFEPointLightElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGFESpecularLightingElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGFESpotLightElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGFETileElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGFETurbulenceElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGFilterElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGFontElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGFontFaceElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGFontFaceFormatElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGFontFaceNameElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGFontFaceSrcElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGFontFaceUriElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGForeignObjectElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGGElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGGlyphElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGGlyphRefElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGGradientElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGHKernElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGImageElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGLength.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGLengthList.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGLinearGradientElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGLineElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGMarkerElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGMaskElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGMatrix.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGMetadataElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGMissingGlyphElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGMPathElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGNumber.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGNumberList.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGPaint.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGPathElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGPathSegArcAbs.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGPathSegArcRel.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGPathSegClosePath.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGPathSegCurvetoCubicAbs.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGPathSegCurvetoCubicRel.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGPathSegCurvetoCubicSmoothAbs.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGPathSegCurvetoCubicSmoothRel.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGPathSegCurvetoQuadraticAbs.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGPathSegCurvetoQuadraticRel.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGPathSegCurvetoQuadraticSmoothAbs.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGPathSegCurvetoQuadraticSmoothRel.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGPathSeg.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGPathSegLinetoAbs.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGPathSegLinetoHorizontalAbs.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGPathSegLinetoHorizontalRel.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGPathSegLinetoRel.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGPathSegLinetoVerticalAbs.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGPathSegLinetoVerticalRel.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGPathSegList.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGPathSegMovetoAbs.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGPathSegMovetoRel.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGPatternElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGPoint.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGPointList.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGPolygonElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGPolylineElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGPreserveAspectRatio.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGRadialGradientElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGRectElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGRect.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGRenderingIntent.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGScriptElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGSetElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGStopElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGStringList.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGStyleElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGSVGElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGSwitchElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGSymbolElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGTextContentElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGTextElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGTextPathElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGTextPositioningElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGTitleElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGTransform.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGTransformList.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGTRefElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGTSpanElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGUnitTypes.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGUseElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGViewElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGVKernElement.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGViewSpec.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGZoomAndPan.idl
	$(GENERATE_BINDINGS_PL) $(WEBCORE_DIR)/svg/SVGZoomEvent.idl

	@# generate-webkit-version.pl
	$(VERBOSE)perl $(WEBCORE_DIR)/../WebKit/scripts/generate-webkitversion.pl --config $(WEBCORE_DIR)/../WebKit/mac/Configurations/Version.xcconfig --outputDir $(dir $@)/

	@# make-css-file-arrays.pl
	$(VERBOSE)perl $(WEBCORE_DIR)/css/make-css-file-arrays.pl $(dir $@)/UserAgentStyleSheets.h $(dir $@)/UserAgentStyleSheetsData.cpp $(WEBCORE_DIR)/css/html.css $(WEBCORE_DIR)/css/quirks.css $(WEBCORE_DIR)/css/mathml.css $(WEBCORE_DIR)/css/svg.css $(WEBCORE_DIR)/css/view-source.css $(WEBCORE_DIR)/css/fullscreen.css $(WEBCORE_DIR)/css/mediaControls.css $(WEBCORE_DIR)/css/mediaControlsQt.css $(WEBCORE_DIR)/css/mediaControlsQtFullscreen.css $(WEBCORE_DIR)/css/themeQtNoListboxes.css $(WEBCORE_DIR)/css/mobileThemeQt.css

	@# make-dom-exceptions.pl	
	$(VERBOSE)perl -I$(WEBCORE_DIR)/bindings/scripts $(WEBCORE_DIR)/dom/make_dom_exceptions.pl --input $(WEBCORE_DIR)/dom/DOMExceptions.in --outputDir $(dir $@)

	@# make_event_factory.pl
	$(VERBOSE)perl -I$(WEBCORE_DIR)/bindings/scripts $(WEBCORE_DIR)/dom/make_event_factory.pl --input $(WEBCORE_DIR)/dom/EventNames.in --outputDir $(dir $@)
	$(VERBOSE)perl -I$(WEBCORE_DIR)/bindings/scripts $(WEBCORE_DIR)/dom/make_event_factory.pl --input $(WEBCORE_DIR)/dom/EventTargetFactory.in --outputDir $(dir $@)

	@# make-hash-tools.pl
	$(VERBOSE)perl $(WEBCORE_DIR)/make-hash-tools.pl $(dir $@) $(WEBCORE_DIR)/platform/ColorData.gperf	

	@# make_names.pl
	$(VERBOSE)perl -I$(WEBCORE_DIR)/bindings/scripts $(WEBCORE_DIR)/dom/make_names.pl --tags $(WEBCORE_DIR)/mathml/mathtags.in --attrs $(WEBCORE_DIR)/mathml/mathattrs.in          --extraDefines $(QT_EXTRA_DEFINES) --preprocessor "$(REP_DIR)/tool/qt5/moc/moc -E" --factory --wrapperFactory --outputDir $(dir $@)
	$(VERBOSE)perl -I$(WEBCORE_DIR)/bindings/scripts $(WEBCORE_DIR)/dom/make_names.pl --tags $(WEBCORE_DIR)/html/HTMLTagNames.in --attrs $(WEBCORE_DIR)/html/HTMLAttributeNames.in --extraDefines $(QT_EXTRA_DEFINES) --preprocessor "$(REP_DIR)/tool/qt5/moc/moc -E" --factory --wrapperFactory --outputDir $(dir $@)
	$(VERBOSE)perl -I$(WEBCORE_DIR)/bindings/scripts $(WEBCORE_DIR)/dom/make_names.pl --fonts $(WEBCORE_DIR)/css/WebKitFontFamilyNames.in --outputDir $(dir $@)
	$(VERBOSE)perl -I$(WEBCORE_DIR)/bindings/scripts $(WEBCORE_DIR)/dom/make_names.pl --tags $(WEBCORE_DIR)/svg/svgtags.in --attrs $(WEBCORE_DIR)/svg/svgattrs.in                  --extraDefines $(QT_EXTRA_DEFINES) --preprocessor "$(REP_DIR)/tool/qt5/moc/moc -E" --factory --wrapperFactory --outputDir $(dir $@)
	$(VERBOSE)perl -I$(WEBCORE_DIR)/bindings/scripts $(WEBCORE_DIR)/dom/make_names.pl --attrs $(WEBCORE_DIR)/xml/xmlnsattrs.in --preprocessor "$(REP_DIR)/tool/qt5/moc/moc -E" --outputDir $(dir $@)
	$(VERBOSE)perl -I$(WEBCORE_DIR)/bindings/scripts $(WEBCORE_DIR)/dom/make_names.pl --attrs $(WEBCORE_DIR)/svg/xlinkattrs.in --preprocessor "$(REP_DIR)/tool/qt5/moc/moc -E" --outputDir $(dir $@)
	$(VERBOSE)perl -I$(WEBCORE_DIR)/bindings/scripts $(WEBCORE_DIR)/dom/make_names.pl --attrs $(WEBCORE_DIR)/xml/xmlattrs.in --preprocessor "$(REP_DIR)/tool/qt5/moc/moc -E" --outputDir $(dir $@)

	@# make_settings.pl
	$(VERBOSE)perl -I$(WEBCORE_DIR)/bindings/scripts $(WEBCORE_DIR)/page/make_settings.pl --input $(WEBCORE_DIR)/page/Settings.in --outputDir $(dir $@)

	@# makeprop.pl
	$(VERBOSE)perl -ne "print $1" $(WEBCORE_DIR)/css/CSSPropertyNames.in $(WEBCORE_DIR)/css/SVGCSSPropertyNames.in > $(dir $@)/CSSPropertyNames.in && cd $(dir $@) && perl -I$(WEBCORE_DIR)/bindings/scripts $(WEBCORE_DIR)/css/makeprop.pl --defines $(QT_DEFINES) --preprocessor "$(REP_DIR)/tool/qt5/moc/moc -E" $(WEBCORE_DIR)/css/CSSPropertyNames.in && $(DEL_FILE) CSSPropertyNames.in CSSPropertyNames.gperf

	@# makegrammar.pl
	$(VERBOSE)perl -I $(WEBCORE_DIR)/bindings/scripts $(WEBCORE_DIR)/css/makegrammar.pl --outputDir $(dir $@) --extraDefines $(QT_EXTRA_DEFINES) --preprocessor "$(REP_DIR)/tool/qt5/moc/moc -E" --symbolsPrefix cssyy $(WEBCORE_DIR)/css/CSSGrammar.y.in

	@# makevalues.pl	
	$(VERBOSE)perl -ne "print $1" $(WEBCORE_DIR)/css/CSSValueKeywords.in $(WEBCORE_DIR)/css/SVGCSSValueKeywords.in > $(dir $@)/CSSValueKeywords.in && cd $(dir $@) && perl -I$(WEBCORE_DIR)/bindings/scripts $(WEBCORE_DIR)/css/makevalues.pl --defines $(QT_DEFINES) --preprocessor "$(REP_DIR)/tool/qt5/moc/moc -E" $(WEBCORE_DIR)/css/CSSValueKeywords.in && $(DEL_FILE) CSSValueKeywords.in CSSValueKeywords.gperf

	@# xxd.pl
	$(VERBOSE)perl $(WEBCORE_DIR)/inspector/xxd.pl InspectorOverlayPage_html $(WEBCORE_DIR)/inspector/InspectorOverlayPage.html $(dir $@)/InspectorOverlayPage.h
	$(VERBOSE)perl $(WEBCORE_DIR)/inspector/xxd.pl InjectedScriptSource_js $(WEBCORE_DIR)/inspector/InjectedScriptSource.js $(dir $@)/InjectedScriptSource.h
	$(VERBOSE)perl $(WEBCORE_DIR)/inspector/xxd.pl InjectedScriptCanvasModuleSource_js $(WEBCORE_DIR)/inspector/InjectedScriptCanvasModuleSource.js $(dir $@)/InjectedScriptCanvasModuleSource.h

	@# CodeGeneratorInspector.py
	$(VERBOSE)python $(WEBCORE_DIR)/inspector/CodeGeneratorInspector.py $(WEBCORE_DIR)/inspector/Inspector.json --output_h_dir $(dir $@) --output_cpp_dir $(dir $@)

	@# create-html-entity-table
	$(VERBOSE)python $(WEBCORE_DIR)/html/parser/create-html-entity-table -o $(dir $@)/HTMLEntityTable.cpp $(WEBCORE_DIR)/html/parser/HTMLEntityNames.in

	$(VERBOSE)touch $@


tools:
	$(VERBOSE)make -C tool/qt5

clean-qt5:
	$(VERBOSE)make -C tool/qt5 clean
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(QT5)
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(QTSCRIPTCLASSIC)
	$(VERBOSE)rm -rf $(REP_DIR)/src/lib/qt5/qtwebkit/Source/JavaScriptCore
	$(VERBOSE)rm -rf $(REP_DIR)/src/lib/qt5/qtwebkit/Source/WebCore/generated
