#
# Build the tools using qmake
#

QMAKESPEC = $(QT5_CONTRIB_DIR)/qtbase/mkspecs/linux-g++

bootstrap/libQtBootstrap.a: bootstrap/Makefile
	$(VERBOSE)QMAKESPEC=$(QMAKESPEC) $(MAKE) -C bootstrap \
	          DEFINES="-DQT_VERSION_STR='\"5.8.0\"' -DQT_VERSION_MAJOR=5 -DQT_VERSION_MINOR=8 -DQT_VERSION_PATCH=0 -DQT_BOOTSTRAPPED -DQT_NO_CAST_TO_ASCII -DQT_NO_FOREACH -DQT_NO_CAST_FROM_ASCII -DQT_BUILD_BOOTSTRAP_LIB -DQT_BUILDING_QT -DQT_ASCII_CAST_WARNINGS -DQT_MOC_COMPAT -DQT_USE_QSTRINGBUILDER -DQT_DEPRECATED_WARNINGS -DQT_DISABLE_DEPRECATED_BEFORE=0x050000 -DQT_NO_EXCEPTIONS -D_LARGEFILE64_SOURCE -D_LARGEFILE_SOURCE -DQT_NO_DEBUG"

moc/moc: bootstrap/libQtBootstrap.a moc/Makefile
	$(VERBOSE)QMAKESPEC=$(QMAKESPEC) $(MAKE) -C moc \
	          DEFINES="-DQT_MOC -DQT_NO_CAST_FROM_ASCII -DQT_NO_CAST_FROM_BYTEARRAY -DQT_NO_COMPRESS -DQT_NO_FOREACH -DQT_USE_QSTRINGBUILDER -DQT_NO_EXCEPTIONS -D_LARGEFILE64_SOURCE -D_LARGEFILE_SOURCE -DQT_NO_DEBUG -DQT_BOOTSTRAP_LIB -DQT_VERSION_STR='\"5.8.0\"' -DQT_VERSION_MAJOR=5 -DQT_VERSION_MINOR=8 -DQT_VERSION_PATCH=0 -DQT_BOOTSTRAPPED -DQT_NO_CAST_TO_ASCII"

rcc/rcc: bootstrap/libQtBootstrap.a rcc/Makefile
	$(VERBOSE)QMAKESPEC=$(QMAKESPEC) $(MAKE) -C rcc \
	          DEFINES="-DQT_RCC -DQT_NO_CAST_FROM_ASCII -DQT_NO_FOREACH -DQT_USE_QSTRINGBUILDER -DQT_NO_EXCEPTIONS -D_LARGEFILE64_SOURCE -D_LARGEFILE_SOURCE -DQT_NO_DEBUG -DQT_BOOTSTRAP_LIB -DQT_VERSION_STR='\"5.8.0\"' -DQT_VERSION_MAJOR=5 -DQT_VERSION_MINOR=8 -DQT_VERSION_PATCH=0 -DQT_BOOTSTRAPPED -DQT_NO_CAST_TO_ASCII"

uic/uic: bootstrap/libQtBootstrap.a uic/Makefile
	$(VERBOSE)QMAKESPEC=$(QMAKESPEC) $(MAKE) -C uic \
	          DEFINES="-DQT_UIC -DQT_NO_CAST_FROM_ASCII -DQT_NO_FOREACH -DQT_UIC_CPP_GENERATOR -DQT_USE_QSTRINGBUILDER -DQT_NO_EXCEPTIONS -D_LARGEFILE64_SOURCE -D_LARGEFILE_SOURCE -DQT_NO_DEBUG -DQT_CORE_LIB"

#
# Rule to generate tool Makefiles from the respective pro files via qmake
#
# The second include path is required to resolve the Genode-specific
# 'gconfig.cpp' file.  Even though this is a 'cpp' file, it is used via
# '#include'. So we have to make its location known to the 'INCLUDEPATH'.
#
%/Makefile: %.pro
	$(VERBOSE)mkdir -p $*/.out
	$(VERBOSE)QMAKESPEC=$(QMAKESPEC) qmake/bin/qmake -o $*/Makefile \
		QT_BUILD_TREE=$(QT5_CONTRIB_DIR)/qtbase \
		QT_CONFIG+=zlib \
		QMAKE_CXXFLAGS=-std=c++11 \
		INCLUDEPATH+=$(QT5_CONTRIB_DIR)/qtbase/include \
		INCLUDEPATH+=$(QT5_CONTRIB_DIR)/qtbase/include \
		INCLUDEPATH+=$(QT5_CONTRIB_DIR)/qtbase/include/QtCore \
		INCLUDEPATH+=$(REP_DIR)/src/lib/qt5/qtbase/src/corelib/global \
		INCLUDEPATH+=$(QT5_CONTRIB_DIR)/qtbase/include/QtCore/5.8.0 \
		INCLUDEPATH+=$(QT5_CONTRIB_DIR)/qtbase/include/QtCore/5.8.0/QtCore \
		INCLUDEPATH+=$(QT5_CONTRIB_DIR)/qtbase/include/QtXml \
		INCLUDEPATH+=$(QT5_CONTRIB_DIR)/qtbase/include/QtXml/5.8.0/QtXml \
		-after DESTDIR= \
		-after "LIBS+=-lQtBootstrap -L../bootstrap" \
		$^

vpath bootstrap.pro $(QT5_CONTRIB_DIR)/qtbase/src/tools/bootstrap
vpath       moc.pro $(QT5_CONTRIB_DIR)/qtbase/src/tools/moc
vpath       rcc.pro $(QT5_CONTRIB_DIR)/qtbase/src/tools/rcc
vpath       uic.pro $(QT5_CONTRIB_DIR)/qtbase/src/tools/uic
