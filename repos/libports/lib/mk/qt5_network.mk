include $(call select_from_repositories,lib/import/import-qt5_network.mk)

SHARED_LIB = yes

# use default warning level to avoid noise when compiling contrib code
CC_WARN =

include $(REP_DIR)/lib/mk/qt5_network_generated.inc

# remove unneeded files to prevent moc warnings
COMPILER_MOC_HEADER_MAKE_ALL_FILES_FILTER_OUT = \
  moc_qbearerengine_p.cpp \
  moc_qbearerplugin_p.cpp \
  moc_qnetworkaccessdebugpipebackend_p.cpp \
  moc_qnetworkconfigmanager.cpp \
  moc_qnetworkconfigmanager_p.cpp \
  moc_qnetworksession.cpp \
  moc_qnetworksession_p.cpp \
  moc_qsslsocket_openssl_p.cpp

COMPILER_MOC_SOURCE_MAKE_ALL_FILES_FILTER_OUT = \

include $(REP_DIR)/lib/mk/qt5.inc

QT5_INC_DIR += $(QT5_CONTRIB_DIR)/qtbase/src/network/access

LIBS += qt5_core zlib libc libssl

CC_CXX_WARN_STRICT =
