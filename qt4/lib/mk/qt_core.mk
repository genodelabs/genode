include $(REP_DIR)/lib/import/import-qt_core.mk

SHARED_LIB = yes

# extracted from src/corelib/Makefile
QT_DEFINES += -DQT_BUILD_CORE_LIB -DQT_NO_USING_NAMESPACE -DQT_NO_CAST_TO_ASCII -DQT_ASCII_CAST_WARNINGS -DQT_MOC_COMPAT -DQT_USE_FAST_OPERATOR_PLUS -DQT_USE_FAST_CONCATENATION -DHB_EXPORT=Q_CORE_EXPORT -DQT_NO_DEBUG

# use default warning level to avoid noise when compiling contrib code
CC_WARN = -Wno-unused-but-set-variable -Wno-deprecated-declarations

include $(REP_DIR)/lib/mk/qt_core_generated.inc

# add Genode-specific sources
QT_SOURCES += qprocess_genode.cpp \
              qeventdispatcher_genode.cpp \
              qmutex_genode.cpp \
              qthread_genode.cpp \
              qwaitcondition_genode.cpp \
              moc_qeventdispatcher_genode_p.cpp

# remove unsupported UNIX-specific files
QT_SOURCES_FILTER_OUT = \
  moc_qeventdispatcher_unix_p.cpp \
  qeventdispatcher_unix.cpp \
  qmutex_unix.cpp \
  qprocess_unix.cpp \
  qthread_unix.cpp \
  qwaitcondition_unix.cpp

# remove unneeded files to prevent moc warnings
COMPILER_MOC_HEADER_MAKE_ALL_FILES_FILTER_OUT = \
  moc_qfuturewatcher.cpp \
  moc_qsharedmemory.cpp

INC_DIR += $(REP_DIR)/include/qt4/QtCore/private \
           $(REP_DIR)/contrib/$(QT4)/include/QtCore/private \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/harfbuzz/src

LIBS += launchpad zlib libc libm alarm libc_lock_pipe

vpath % $(REP_DIR)/include/qt4/QtCore
vpath % $(REP_DIR)/include/qt4/QtCore/private

vpath % $(REP_DIR)/src/lib/qt4/src/corelib/global
vpath % $(REP_DIR)/src/lib/qt4/src/3rdparty/harfbuzz/src
vpath % $(REP_DIR)/src/lib/qt4/src/corelib/animation
vpath % $(REP_DIR)/src/lib/qt4/src/corelib/concurrent
vpath % $(REP_DIR)/src/lib/qt4/src/corelib/thread
vpath % $(REP_DIR)/src/lib/qt4/src/corelib/tools
vpath % $(REP_DIR)/src/lib/qt4/src/corelib/io
vpath % $(REP_DIR)/src/lib/qt4/src/corelib/plugin
vpath % $(REP_DIR)/src/lib/qt4/src/corelib/kernel
vpath % $(REP_DIR)/src/lib/qt4/src/corelib/statemachine
vpath % $(REP_DIR)/src/lib/qt4/src/corelib/xml
vpath % $(REP_DIR)/src/lib/qt4/src/corelib/codecs
vpath % $(REP_DIR)/src/lib/qt4/src/plugins/codecs/cn
vpath % $(REP_DIR)/src/lib/qt4/src/plugins/codecs/jp
vpath % $(REP_DIR)/src/lib/qt4/src/plugins/codecs/kr
vpath % $(REP_DIR)/src/lib/qt4/src/plugins/codecs/tw

vpath % $(REP_DIR)/contrib/$(QT4)/src/corelib/global
vpath % $(REP_DIR)/contrib/$(QT4)/src/3rdparty/harfbuzz/src
vpath % $(REP_DIR)/contrib/$(QT4)/src/corelib/animation
vpath % $(REP_DIR)/contrib/$(QT4)/src/corelib/concurrent
vpath % $(REP_DIR)/contrib/$(QT4)/src/corelib/thread
vpath % $(REP_DIR)/contrib/$(QT4)/src/corelib/tools
vpath % $(REP_DIR)/contrib/$(QT4)/src/corelib/io
vpath % $(REP_DIR)/contrib/$(QT4)/src/corelib/plugin
vpath % $(REP_DIR)/contrib/$(QT4)/src/corelib/kernel
vpath % $(REP_DIR)/contrib/$(QT4)/src/corelib/statemachine
vpath % $(REP_DIR)/contrib/$(QT4)/src/corelib/xml
vpath % $(REP_DIR)/contrib/$(QT4)/src/corelib/codecs
vpath % $(REP_DIR)/contrib/$(QT4)/src/plugins/codecs/cn
vpath % $(REP_DIR)/contrib/$(QT4)/src/plugins/codecs/jp
vpath % $(REP_DIR)/contrib/$(QT4)/src/plugins/codecs/kr
vpath % $(REP_DIR)/contrib/$(QT4)/src/plugins/codecs/tw

include $(REP_DIR)/lib/mk/qt.inc
