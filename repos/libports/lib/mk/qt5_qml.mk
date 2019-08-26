include $(call select_from_repositories,lib/import/import-qt5_qml.mk)

SHARED_LIB = yes

include $(REP_DIR)/lib/mk/qt5_qml_generated.inc

# remove unneeded files to prevent moc warnings
COMPILER_MOC_HEADER_MAKE_ALL_FILES_FILTER_OUT = \
  moc_qqmlabstractprofileradapter_p.cpp \
  moc_qqmldebugconnector_p.cpp \
  moc_qqmldebugservice_p.cpp \
  moc_qqmldebugserviceinterfaces_p.cpp \
  moc_qqmlprofiler_p.cpp \
  moc_qv4debugging_p.cpp \
  moc_qv4profiling_p.cpp \


QT_VPATH += qtdeclarative/src/qml

include $(REP_DIR)/lib/mk/qt5.inc

LIBS += qt5_network qt5_core libc

CC_CXX_WARN_STRICT =
