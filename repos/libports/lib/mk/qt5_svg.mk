include $(REP_DIR)/lib/import/import-qt5_svg.mk

SHARED_LIB = yes

LIBS	+= mesa zlib

include $(REP_DIR)/lib/mk/qt5_svg_generated.inc

include $(REP_DIR)/lib/mk/qt5.inc

INC_DIR += $(QT5_CONTRIB_DIR)/qtsvg/include/QtSvg/$(QT_VERSION)/QtSvg \
