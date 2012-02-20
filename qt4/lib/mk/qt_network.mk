include $(REP_DIR)/lib/import/import-qt_network.mk

SHARED_LIB = yes

# extracted from src/script/Makefile
QT_DEFINES += -DQT_BUILD_NETWORK_LIB -DQT_NO_USING_NAMESPACE -DQT_NO_CAST_TO_ASCII -DQT_ASCII_CAST_WARNINGS -DQT_MOC_COMPAT -DQT_USE_FAST_OPERATOR_PLUS -DQT_USE_FAST_CONCATENATION -DQT_NO_DEBUG -DQT_CORE_LIB

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

INC_DIR += $(REP_DIR)/include/qt4/QtNetwork/private \
           $(REP_DIR)/contrib/$(QT4)/include/QtNetwork/private

LIBS += qt_core zlib libc

vpath % $(REP_DIR)/include/qt4/QtNetwork
vpath % $(REP_DIR)/include/qt4/QtNetwork/private

vpath % $(REP_DIR)/src/lib/qt4/src/network/access
vpath % $(REP_DIR)/src/lib/qt4/src/network/bearer
vpath % $(REP_DIR)/src/lib/qt4/src/network/kernel
vpath % $(REP_DIR)/src/lib/qt4/src/network/socket

vpath % $(REP_DIR)/contrib/$(QT4)/src/network/access
vpath % $(REP_DIR)/contrib/$(QT4)/src/network/bearer
vpath % $(REP_DIR)/contrib/$(QT4)/src/network/kernel
vpath % $(REP_DIR)/contrib/$(QT4)/src/network/socket

include $(REP_DIR)/lib/mk/qt.inc
