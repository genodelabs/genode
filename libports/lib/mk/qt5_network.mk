include $(REP_DIR)/lib/import/import-qt5_network.mk

SHARED_LIB = yes

# use default warning level to avoid noise when compiling contrib code
CC_WARN =

include $(REP_DIR)/lib/mk/qt5_network_generated.inc

# remove unneeded files to prevent moc warnings
COMPILER_MOC_HEADER_MAKE_ALL_FILES_FILTER_OUT = \
  moc_qftp_p.cpp \
  moc_qnetworkaccessdebugpipebackend_p.cpp \
  moc_qnetworkaccessftpbackend_p.cpp \
  moc_qnetworksession.cpp \
  moc_qnetworkconfigmanager.cpp \
  moc_qnetworkconfigmanager_p.cpp \
  moc_qnetworksession_p.cpp \
  moc_qbearerengine_p.cpp \
  moc_qbearerplugin_p.cpp \
  moc_qudpsocket.cpp \
  moc_qsslsocket_openssl_p.cpp \
  

COMPILER_MOC_SOURCE_MAKE_ALL_FILES_FILTER_OUT = \
  qftp.moc

include $(REP_DIR)/lib/mk/qt5.inc

INC_DIR += $(REP_DIR)/contrib/$(QT5)/qtbase/include/QtNetwork/$(QT_VERSION) \
           $(REP_DIR)/contrib/$(QT5)/qtbase/include/QtNetwork/$(QT_VERSION)/QtNetwork \
           $(REP_DIR)/contrib/$(QT5)/qtbase/include/QtCore/$(QT_VERSION) \
           $(REP_DIR)/contrib/$(QT5)/qtbase/include/QtCore/$(QT_VERSION)/QtCore

LIBS += qt5_core zlib libc libssl
