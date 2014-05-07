include $(REP_DIR)/lib/import/import-qt_core.mk

SHARED_LIB = yes

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

include $(REP_DIR)/lib/mk/qt.inc

INC_DIR += $(REP_DIR)/include/qt4/QtCore/private \
           $(REP_DIR)/contrib/$(QT4)/include/QtCore/private

LIBS += launchpad zlib libc libm alarm libc_lock_pipe

vpath % $(REP_DIR)/include/qt4/QtCore
vpath % $(REP_DIR)/include/qt4/QtCore/private
