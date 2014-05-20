#
# \brief  Makefile for building the Qt4 tools
# \author Christian Prochaska
# \author Norman Feske
# \date   2009-05-15
#

REP_DIR := $(realpath ../..)

include $(REP_DIR)/lib/mk/qt5_version.inc

#
# Compound rule for building the tools in the right order
#
all: qmake/qmake moc/moc rcc/rcc uic/uic

QMAKESPEC = $(REP_DIR)/contrib/$(QT5)/qtbase/mkspecs/linux-g++

#
# Build qmake
#
qmake/qmake:
	QMAKESPEC=$(QMAKESPEC) make -C qmake

#
# Build the other tools using qmake
#
vpath bootstrap.pro $(REP_DIR)/contrib/$(QT5)/qtbase/src/tools/bootstrap
vpath       moc.pro $(REP_DIR)/contrib/$(QT5)/qtbase/src/tools/moc
vpath       rcc.pro $(REP_DIR)/contrib/$(QT5)/qtbase/src/tools/rcc
vpath       uic.pro $(REP_DIR)/contrib/$(QT5)/qtbase/src/tools/uic

#
# The Makefile needs to rebuild itself to get the correct source paths.
# This gets done by the 'qmake' target.
#

bootstrap/libQtBootstrap.a: bootstrap/Makefile
	QMAKESPEC=$(QMAKESPEC) make -C bootstrap qmake
	QMAKESPEC=$(QMAKESPEC) make -C bootstrap

moc/moc: bootstrap/libQtBootstrap.a moc/Makefile
	QMAKESPEC=$(QMAKESPEC) make -C moc qmake
	QMAKESPEC=$(QMAKESPEC) make -C moc

rcc/rcc: rcc/Makefile bootstrap/libQtBootstrap.a
	QMAKESPEC=$(QMAKESPEC) make -C rcc qmake
	QMAKESPEC=$(QMAKESPEC) make -C rcc

uic/uic: uic/Makefile bootstrap/libQtBootstrap.a
	QMAKESPEC=$(QMAKESPEC) make -C uic qmake
	QMAKESPEC=$(QMAKESPEC) make -C uic

#
# Rule to generate tool Makefiles from the respective pro files via qmake
#
# The second include path is required to resolve the Genode-specific
# 'gconfig.cpp' file.  Even though this is a 'cpp' file, it is used via
# '#include'. So we have to make its location known to the 'INCLUDEPATH'.
#
%/Makefile: %.pro
	QMAKESPEC=$(QMAKESPEC) qmake/qmake -o $*/Makefile \
		QT_BUILD_TREE=$(REP_DIR)/contrib/$(QT5)/qtbase \
		QT_CONFIG+=zlib \
		INCLUDEPATH+=$(REP_DIR)/include/qt5 \
		INCLUDEPATH+=$(REP_DIR)/contrib/$(QT5)/qtbase/include \
		INCLUDEPATH+=$(REP_DIR)/include/qt5/qtbase \
		INCLUDEPATH+=$(REP_DIR)/contrib/$(QT5)/qtbase/include \
		INCLUDEPATH+=$(REP_DIR)/include/qt5/qtbase/QtCore \
		INCLUDEPATH+=$(REP_DIR)/contrib/$(QT5)/qtbase/include/QtCore \
		INCLUDEPATH+=$(REP_DIR)/src/lib/qt5/qtbase/src/corelib/global \
		INCLUDEPATH+=$(REP_DIR)/contrib/$(QT5)/qtbase/include/QtCore/5.1.0 \
		INCLUDEPATH+=$(REP_DIR)/contrib/$(QT5)/qtbase/include/QtCore/5.1.0/QtCore \
		INCLUDEPATH+=$(REP_DIR)/include/qt5/qtbase/QtXml \
		INCLUDEPATH+=$(REP_DIR)/contrib/$(QT5)/qtbase/include/QtXml \
		-after DESTDIR= \
		-after "LIBS+=-lQtBootstrap -L../bootstrap" \
		$^

#
# Clean rule
#
clean:
	make -C qmake clean
	rm -rf bootstrap moc rcc uic

distclean: clean
