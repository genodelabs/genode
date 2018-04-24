include $(call select_from_repositories,lib/import/import-qt5_svg.mk)

SHARED_LIB = yes

LIBS += qt5_core qt5_gui qt5_widgets zlib

include $(REP_DIR)/lib/mk/qt5_svg_generated.inc

include $(REP_DIR)/lib/mk/qt5.inc

CC_CXX_WARN_STRICT =
