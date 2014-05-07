include $(REP_DIR)/lib/import/import-qt_network.mk

SHARED_LIB = yes

# use default warning level to avoid noise when compiling contrib code
CC_WARN =

include $(REP_DIR)/lib/mk/qt_network_generated.inc

# remove unneeded files to prevent moc warnings
COMPILER_MOC_HEADER_MAKE_ALL_FILES_FILTER_OUT = \
  moc_qftp.cpp \
  moc_qnetworkaccessdebugpipebackend_p.cpp \
  moc_qnetworkaccessftpbackend_p.cpp \
  moc_qudpsocket.cpp

COMPILER_MOC_SOURCE_MAKE_ALL_FILES_FILTER_OUT = \
  qftp.moc

include $(REP_DIR)/lib/mk/qt.inc

LIBS += qt_core zlib libc libssl
