include $(call select_from_repositories,lib/import/import-qt5_gui.mk)

SHARED_LIB = yes

ifeq ($(filter-out $(SPECS),x86),)
CC_OPT += -mno-sse2
endif

# use default warning level to avoid noise when compiling contrib code
CC_WARN = -Wno-unused-but-set-variable -Wno-deprecated-declarations

include $(REP_DIR)/lib/mk/qt5_gui_generated.inc

QT_SOURCES_FILTER_OUT = \
  qdrawhelper_sse2.cpp \
  qimage_sse2.cpp

ifeq ($(filter-out $(SPECS),arm_64),)
QT_DEFINES += -UENABLE_PIXMAN_DRAWHELPERS
QT_SOURCES += qdrawhelper_neon.cpp \
              qimage_neon.cpp \
              qimagescale_neon.cpp
endif

# remove unneeded files to prevent moc warnings
COMPILER_MOC_HEADER_MAKE_ALL_FILES_FILTER_OUT = \
  moc_qsessionmanager.cpp \
  qrc_qmake_webgradients.cpp

include $(REP_DIR)/lib/mk/qt5.inc

LIBS += qt5_core zlib libpng

CC_CXX_WARN_STRICT =
