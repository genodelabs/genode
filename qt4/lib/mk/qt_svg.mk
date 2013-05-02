include $(REP_DIR)/lib/import/import-qt_svg.mk

SHARED_LIB = yes

include $(REP_DIR)/lib/mk/qt_svg_generated.inc

include $(REP_DIR)/lib/mk/qt.inc

INC_DIR += $(REP_DIR)/include/qt4/QtSvg/private \
           $(REP_DIR)/contrib/$(QT4)/include/QtSvg/private

LIBS += qt_core qt_gui zlib libc

vpath % $(REP_DIR)/include/qt4/QtSvg
vpath % $(REP_DIR)/include/qt4/QtSvg/private
