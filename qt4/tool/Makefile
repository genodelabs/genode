#
# \brief  Makefile for building the Qt4 tools
# \author Christian Prochaska
# \author Norman Feske
# \date   2009-05-15
#

REP_DIR := $(realpath ..)

include $(REP_DIR)/lib/mk/qt_version.inc

#
# Compound rule for building the tools in the right order
#
all: qmake/qmake moc/moc rcc/rcc uic/uic

#
# Determine qmakespec to be passed to the sub makefiles
#
HOST_ARCH := $(shell uname -m)
ifneq ($(HOST_ARCH),x86_64)
HOST_ARCH=x86
endif

QMAKESPEC = $(REP_DIR)/contrib/$(QT4)/mkspecs/qws/linux-$(HOST_ARCH)-g++

#
# Build qmake
#
qmake/qmake:
	QMAKESPEC=$(QMAKESPEC) make -C qmake

#
# Build the other tools using qmake
#
vpath bootstrap.pro $(REP_DIR)/contrib/$(QT4)/src/tools/bootstrap
vpath       moc.pro $(REP_DIR)/contrib/$(QT4)/src/tools/moc
vpath       rcc.pro $(REP_DIR)/contrib/$(QT4)/src/tools/rcc
vpath       uic.pro $(REP_DIR)/contrib/$(QT4)/src/tools/uic

bootstrap/libbootstrap.a: bootstrap/Makefile
	make -C bootstrap

moc/moc: moc/Makefile bootstrap/libbootstrap.a
	make -C moc

rcc/rcc: rcc/Makefile bootstrap/libbootstrap.a
	make -C rcc

uic/uic: uic/Makefile bootstrap/libbootstrap.a
	make -C uic

#
# Rule to generate tool Makefiles from the respective pro files via qmake
#
# The second include path is required to resolve the Genode-specific
# 'gconfig.cpp' file.  Even though this is a 'cpp' file, it is used via
# '#include'. So we have to make its location known to the 'INCLUDEPATH'.
#
%/Makefile: %.pro
	mkdir -p $*
	QMAKESPEC=$(QMAKESPEC) qmake/qmake -o $*/Makefile \
		QT_BUILD_TREE=$(REP_DIR)/contrib/$(QT4) \
		INCLUDEPATH+=$(REP_DIR)/include/qt4 \
		INCLUDEPATH+=$(REP_DIR)/include/qt4/QtCore \
		INCLUDEPATH+=$(REP_DIR)/src/lib/qt4/src/corelib/global \
		-after DESTDIR= \
		-after LIBS+=-L../bootstrap\
		$^

#
# Clean rule
#
clean:
	make -C qmake clean
	rm -rf bootstrap moc rcc uic

distclean: clean
