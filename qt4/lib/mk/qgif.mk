QT4 = qt-everywhere-opensource-src-4.7.1

SRC_CC       = main.cpp qgifhandler.cpp

INC_DIR     += $(REP_DIR)/contrib/$(QT4)/include/QtGui/private

LIBS         = qt_core libc

vpath % $(call select_from_repositories,contrib/$(QT4)/src/plugins/imageformats/gif)
vpath % $(call select_from_repositories,contrib/$(QT4)/src/gui/image)
