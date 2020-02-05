include $(call select_from_repositories,lib/import/import-qt5_core.mk)

SHARED_LIB = yes

# use default warning level to avoid noise when compiling contrib code
CC_WARN = -Wno-unused-but-set-variable -Wno-deprecated-declarations

include $(REP_DIR)/lib/mk/qt5_core_generated.inc

# remove unsupported UNIX-specific files
QT_SOURCES_FILTER_OUT = \
  forkfd_qt.cpp \
  moc_qfilesystemwatcher_inotify_p.cpp \
  qfilesystemwatcher_inotify.cpp

# remove unneeded files to prevent moc warnings
COMPILER_MOC_HEADER_MAKE_ALL_FILES_FILTER_OUT = \

include $(REP_DIR)/lib/mk/qt5.inc

# reduce 'not implemented yet' noise
SRC_CC += libc_dummies.cc
vpath libc_dummies.cc $(REP_DIR)/src/lib/qt5

LIBS += zlib qt5_pcre2 libc libm libc_pipe

CC_CXX_WARN_STRICT =
