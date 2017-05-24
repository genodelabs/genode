include $(REP_DIR)/lib/import/import-qt5_gui.mk

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

# remove unneeded files to prevent moc warnings
COMPILER_MOC_HEADER_MAKE_ALL_FILES_FILTER_OUT = \
  moc_qsessionmanager.cpp

include $(REP_DIR)/lib/mk/qt5.inc

INC_DIR += $(REP_DIR)/include/qt5/qtbase/QtGui/private \
           $(QT5_CONTRIB_DIR)/qtbase/include/QtGui/$(QT_VERSION) \
           $(QT5_CONTRIB_DIR)/qtbase/include/QtGui/$(QT_VERSION)/QtGui \
           $(QT5_CONTRIB_DIR)/qtbase/include/QtGui/$(QT_VERSION)/QtGui/private \
           $(QT5_CONTRIB_DIR)/qtbase/include/QtCore/$(QT_VERSION) \
           $(QT5_CONTRIB_DIR)/qtbase/include/QtCore/$(QT_VERSION)/QtCore

LIBS += qt5_core zlib libpng

#
# install fonts
#

ifneq ($(call select_from_ports,qt5),)
all: $(BUILD_BASE_DIR)/bin/qt5_fs/qt/lib/fonts
endif

$(BUILD_BASE_DIR)/bin/qt5_fs/qt/lib/fonts:
	$(VERBOSE)mkdir -p $@
	$(VERBOSE)ln -sf $(QT5_CONTRIB_DIR)/qtquickcontrols/examples/quickcontrols/extras/dashboard/fonts/DejaVuSans.ttf $@/
