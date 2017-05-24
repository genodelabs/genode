include $(REP_DIR)/lib/import/import-qt5_printsupport.mk

SHARED_LIB = yes

# use default warning level to avoid noise when compiling contrib code
CC_WARN =

include $(REP_DIR)/lib/mk/qt5_printsupport_generated.inc

# remove unneeded files to prevent moc warnings
COMPILER_MOC_HEADER_MAKE_ALL_FILES_FILTER_OUT = \

COMPILER_MOC_SOURCE_MAKE_ALL_FILES_FILTER_OUT = \

# UI headers
moc_qpagesetupdialog_unix_p.o: ui_qpagesetupwidget.h
qprintdialog_unix.o:           ui_qprintpropertieswidget.h
qprintdialog_unix.o:           ui_qprintsettingsoutput.h
qprintdialog_unix.o:           ui_qprintwidget.h

include $(REP_DIR)/lib/mk/qt5.inc

INC_DIR += $(QT5_CONTRIB_DIR)/qtbase/include/QtPrintSupport/$(QT_VERSION)/QtPrintSupport \
           $(QT5_CONTRIB_DIR)/qtbase/include/QtWidgets/$(QT_VERSION)/QtWidgets \
           $(QT5_CONTRIB_DIR)/qtbase/include/QtGui/$(QT_VERSION)/QtGui \
           $(QT5_CONTRIB_DIR)/qtbase/include/QtCore/$(QT_VERSION)/QtCore \

LIBS += qt5_gui
