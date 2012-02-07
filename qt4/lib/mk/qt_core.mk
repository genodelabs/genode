include $(REP_DIR)/lib/import/import-qt_core.mk

SHARED_LIB = yes

# extracted from src/corelib/Makefile
QT_DEFINES += -DQT_BUILD_CORE_LIB -DQT_NO_USING_NAMESPACE -DQT_NO_CAST_TO_ASCII -DQT_ASCII_CAST_WARNINGS -DQT_MOC_COMPAT -DQT_USE_FAST_OPERATOR_PLUS -DQT_USE_FAST_CONCATENATION -DHB_EXPORT=Q_CORE_EXPORT -DQT_NO_DEBUG

# use default warning level to avoid noise when compiling contrib code
CC_WARN = -Wno-unused-but-set-variable -Wno-deprecated-declarations

# extracted from src/corelib/Makefile
QT_SOURCES = \
         qabstractanimation.cpp \
         qvariantanimation.cpp \
         qpropertyanimation.cpp \
         qanimationgroup.cpp \
         qsequentialanimationgroup.cpp \
         qparallelanimationgroup.cpp \
         qpauseanimation.cpp \
         qfuture.cpp \
         qfutureinterface.cpp \
         qfuturesynchronizer.cpp \
         qfuturewatcher.cpp \
         qrunnable.cpp \
         qtconcurrentfilter.cpp \
         qtconcurrentmap.cpp \
         qtconcurrentresultstore.cpp \
         qtconcurrentthreadengine.cpp \
         qtconcurrentiteratekernel.cpp \
         qtconcurrentexception.cpp \
         qthreadpool.cpp \
         qglobal.cpp \
         qlibraryinfo.cpp \
         qmalloc.cpp \
         qnumeric.cpp \
         qatomic.cpp \
         qmutex.cpp \
         qreadwritelock.cpp \
         qmutexpool.cpp \
         qsemaphore.cpp \
         qthread.cpp \
         qthreadstorage.cpp \
         qbitarray.cpp \
         qbytearray.cpp \
         qbytearraymatcher.cpp \
         qcryptographichash.cpp \
         qdatetime.cpp \
         qeasingcurve.cpp \
         qelapsedtimer.cpp \
         qhash.cpp \
         qline.cpp \
         qlinkedlist.cpp \
         qlist.cpp \
         qlocale.cpp \
         qpoint.cpp \
         qmap.cpp \
         qmargins.cpp \
         qcontiguouscache.cpp \
         qrect.cpp \
         qregexp.cpp \
         qshareddata.cpp \
         qsharedpointer.cpp \
         qsimd.cpp \
         qsize.cpp \
         qstring.cpp \
         qstringbuilder.cpp \
         qstringlist.cpp \
         qtextboundaryfinder.cpp \
         qtimeline.cpp \
         qvector.cpp \
         qvsnprintf.cpp \
         qelapsedtimer_unix.cpp \
         harfbuzz-buffer.c \
         harfbuzz-gdef.c \
         harfbuzz-gsub.c \
         harfbuzz-gpos.c \
         harfbuzz-impl.c \
         harfbuzz-open.c \
         harfbuzz-stream.c \
         harfbuzz-shaper-all.cpp \
         qharfbuzz.cpp \
         qabstractfileengine.cpp \
         qbuffer.cpp \
         qdatastream.cpp \
         qdataurl.cpp \
         qdebug.cpp \
         qdir.cpp \
         qdiriterator.cpp \
         qfile.cpp \
         qfileinfo.cpp \
         qiodevice.cpp \
         qnoncontiguousbytedevice.cpp \
         qprocess.cpp \
         qtextstream.cpp \
         qtemporaryfile.cpp \
         qresource.cpp \
         qresource_iterator.cpp \
         qurl.cpp \
         qsettings.cpp \
         qfsfileengine.cpp \
         qfsfileengine_iterator.cpp \
         qfilesystemwatcher.cpp \
         qfsfileengine_unix.cpp \
         qfsfileengine_iterator_unix.cpp \
         qpluginloader.cpp \
         qfactoryloader.cpp \
         quuid.cpp \
         qlibrary.cpp \
         qlibrary_unix.cpp \
         qabstracteventdispatcher.cpp \
         qabstractitemmodel.cpp \
         qbasictimer.cpp \
         qeventloop.cpp \
         qcoreapplication.cpp \
         qcoreevent.cpp \
         qmetaobject.cpp \
         qmetatype.cpp \
         qmimedata.cpp \
         qobject.cpp \
         qobjectcleanuphandler.cpp \
         qsignalmapper.cpp \
         qsocketnotifier.cpp \
         qtimer.cpp \
         qtranslator.cpp \
         qvariant.cpp \
         qcoreglobaldata.cpp \
         qsystemsemaphore.cpp \
         qpointer.cpp \
         qmath.cpp \
         qcore_unix.cpp \
         qisciicodec.cpp \
         qlatincodec.cpp \
         qsimplecodec.cpp \
         qtextcodec.cpp \
         qtsciicodec.cpp \
         qutfcodec.cpp \
         qtextcodecplugin.cpp \
         qfontlaocodec.cpp \
         qgb18030codec.cpp \
         qjpunicode.cpp \
         qeucjpcodec.cpp \
         qjiscodec.cpp \
         qsjiscodec.cpp \
         qeuckrcodec.cpp \
         qbig5codec.cpp \
         qfontjpcodec.cpp \
         qstatemachine.cpp \
         qabstractstate.cpp \
         qstate.cpp \
         qfinalstate.cpp \
         qhistorystate.cpp \
         qabstracttransition.cpp \
         qsignaltransition.cpp \
         qeventtransition.cpp \
         qxmlstream.cpp \
         qxmlutils.cpp \
         moc_qthreadpool.cpp \
         moc_qnamespace.cpp \
         moc_qthread.cpp \
         moc_qeasingcurve.cpp \
         moc_qlocale.cpp \
         moc_qtimeline.cpp \
         moc_qfile.cpp \
         moc_qiodevice.cpp \
         moc_qnoncontiguousbytedevice_p.cpp \
         moc_qtemporaryfile.cpp \
         moc_qsettings.cpp \
         moc_qfilesystemwatcher_p.cpp \
         moc_qpluginloader.cpp \
         moc_qlibrary.cpp \
         moc_qfactoryloader_p.cpp \
         moc_qabstracteventdispatcher.cpp \
         moc_qabstractitemmodel.cpp \
         moc_qeventloop.cpp \
         moc_qcoreapplication.cpp \
         moc_qcoreevent.cpp \
         moc_qmimedata.cpp \
         moc_qsocketnotifier.cpp \
         moc_qtimer.cpp \
         moc_qtranslator.cpp \
         moc_qobjectcleanuphandler.cpp \
         moc_qtextcodecplugin.cpp \
         moc_qabstractstate.cpp \
         moc_qstate.cpp \
         moc_qfinalstate.cpp \
         moc_qhistorystate.cpp \
         moc_qabstracttransition.cpp \
         moc_qsignaltransition.cpp \
         moc_qeventtransition.cpp

# UNIX-specific files removed
#         qcrashhandler.cpp \
#         qprocess_unix.cpp \
#         qeventdispatcher_unix.cpp \
#         qmutex_unix.cpp \
#         qthread_unix.cpp \
#         qwaitcondition_unix.cpp \
#         qsharedmemory_unix.cpp \
#         qsystemsemaphore_unix.cpp \
#         moc_qeventdispatcher_unix_p.cpp \

# add Genode-specific sources
QT_SOURCES += qprocess_genode.cpp \
              qeventdispatcher_genode.cpp \
              qmutex_genode.cpp \
              qthread_genode.cpp \
              qwaitcondition_genode.cpp \
              moc_qeventdispatcher_genode_p.cpp

# some source files need to be generated by moc from other source/header files before
# they get #included again by the original source file in the compiling stage

# source files generated from existing header files ("moc_%.cpp: %.h" rule in spec-qt4.mk)
# extracted from "compiler_moc_header_make_all" target
COMPILER_MOC_HEADER_MAKE_ALL_FILES = \
                                     moc_qabstractanimation.cpp \
                                     moc_qvariantanimation.cpp \
                                     moc_qpropertyanimation.cpp \
                                     moc_qanimationgroup.cpp \
                                     moc_qsequentialanimationgroup.cpp \
                                     moc_qparallelanimationgroup.cpp \
                                     moc_qpauseanimation.cpp \
                                     moc_qthreadpool.cpp \
                                     moc_qnamespace.cpp \
                                     moc_qthread.cpp \
                                     moc_qeasingcurve.cpp \
                                     moc_qlocale.cpp \
                                     moc_qtimeline.cpp \
                                     moc_qbuffer.cpp \
                                     moc_qfile.cpp \
                                     moc_qiodevice.cpp \
                                     moc_qnoncontiguousbytedevice_p.cpp \
                                     moc_qprocess.cpp \
                                     moc_qtemporaryfile.cpp \
                                     moc_qsettings.cpp \
                                     moc_qfilesystemwatcher.cpp \
                                     moc_qfilesystemwatcher_p.cpp \
                                     moc_qpluginloader.cpp \
                                     moc_qlibrary.cpp \
                                     moc_qfactoryloader_p.cpp \
                                     moc_qabstracteventdispatcher.cpp \
                                     moc_qabstractitemmodel.cpp \
                                     moc_qeventloop.cpp \
                                     moc_qcoreapplication.cpp \
                                     moc_qcoreevent.cpp \
                                     moc_qmimedata.cpp \
                                     moc_qobject.cpp \
                                     moc_qsignalmapper.cpp \
                                     moc_qsocketnotifier.cpp \
                                     moc_qtimer.cpp \
                                     moc_qtranslator.cpp \
                                     moc_qobjectcleanuphandler.cpp \
                                     moc_qeventdispatcher_unix_p.cpp \
                                     moc_qtextcodecplugin.cpp \
                                     moc_qstatemachine.cpp

# source files generated from existing source files ("%.moc: %.cpp" rule in spec-qt4.mk)
# extracted from "compiler_moc_source_make_all" rule
COMPILER_MOC_SOURCE_MAKE_ALL_FILES = \
                                     qtextstream.moc \
                                     qfilesystemwatcher.moc \
                                     qtimer.moc

INC_DIR += $(REP_DIR)/src/lib/qt4/mkspecs/qws/genode-x86-g++ \
           $(REP_DIR)/include/qt4/QtCore/private \
           $(REP_DIR)/contrib/$(QT4)/include/QtCore/private \
           $(REP_DIR)/src/lib/qt4/src/corelib/global \
           $(REP_DIR)/contrib/$(QT4)/src/corelib/codecs \
           $(REP_DIR)/contrib/$(QT4)/src/3rdparty/harfbuzz/src

LIBS += launchpad server cxx env thread zlib libc libm alarm libc_lock_pipe

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

include $(REP_DIR)/lib/mk/qt.mk
