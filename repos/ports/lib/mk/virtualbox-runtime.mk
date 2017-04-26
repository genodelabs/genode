include $(REP_DIR)/lib/mk/virtualbox-common.inc

INC_DIR += $(VBOX_DIR)/Runtime/include

INC_DIR += $(VIRTUALBOX_DIR)/src/libs/liblzf-3.4
INC_DIR += $(VIRTUALBOX_DIR)/src/libs/zlib-1.2.8
INC_DIR += $(call select_from_ports,libiconv)/include/iconv

GENERIC_SRC_CC = $(notdir $(wildcard $(VBOX_DIR)/Runtime/generic/*.cpp))

FILTERED_OUT_SRC_CC = RTLogDefaultInit-generic.cpp \
                      RTTimeLocalExplode-generic.cpp \
                      semrw-lockless-generic.cpp \
                      tls-generic.cpp \
                      fs-stubs-generic.cpp \
                      http-curl.cpp \
                      RTSemEventMultiWait-2-ex-generic.cpp \
                      RTLogWriteStdErr-generic.cpp \
                      RTLogWriteStdOut-generic.cpp \
                      RTMpGetDescription-generic-stub.cpp \
                      RTSemEventWait-2-ex-generic.cpp \
                      RTSemEventWait-generic.cpp \
                      RTSemEventWaitNoResume-2-ex-generic.cpp \
                      RTFileExists-generic.cpp \
                      RTSemMutexRequest-generic.cpp \
                      RTSemMutexRequestDebug-generic.cpp \
                      RTDirExists-generic.cpp \
                      RTMpOnPair-generic.cpp \
                      RTTimerCreate-generic.cpp \
                      timer-generic.cpp

CC_WARN += -Wno-unused-variable

SRC_CC += Runtime/common/log/logrel.cpp \
          Runtime/r3/init.cpp \
          Runtime/common/misc/thread.cpp \
          $(addprefix Runtime/generic/,$(filter-out $(FILTERED_OUT_SRC_CC), $(GENERIC_SRC_CC)))

SRC_CC += Runtime/common/err/RTErrConvertFromErrno.cpp
SRC_CC += Runtime/common/alloc/memcache.cpp
SRC_CC += Runtime/common/alloc/heapoffset.cpp
SRC_CC += Runtime/common/checksum/alt-md5.cpp
SRC_CC += Runtime/common/checksum/alt-sha512.cpp
SRC_CC += Runtime/common/checksum/crc32.cpp
SRC_CC += Runtime/common/checksum/ipv4.cpp
SRC_CC += Runtime/common/checksum/sha512str.cpp
SRC_CC += Runtime/common/log/log.cpp
SRC_CC += Runtime/common/log/log.cpp
SRC_CC += Runtime/common/log/logellipsis.cpp
SRC_CC += Runtime/common/log/logrelellipsis.cpp
SRC_CC += Runtime/common/log/logformat.cpp
SRC_CC += Runtime/common/misc/assert.cpp
SRC_CC += Runtime/common/misc/buildconfig.cpp
SRC_CC += Runtime/common/misc/lockvalidator.cpp
SRC_CC += Runtime/common/misc/once.cpp
SRC_CC += Runtime/common/misc/req.cpp
SRC_CC += Runtime/common/misc/reqpool.cpp
SRC_CC += Runtime/common/misc/reqqueue.cpp
SRC_CC += Runtime/common/misc/sg.cpp
SRC_CC += Runtime/common/misc/term.cpp
SRC_CC += Runtime/common/misc/RTAssertMsg1Weak.cpp
SRC_CC += Runtime/common/misc/RTAssertMsg2AddWeak.cpp
SRC_CC += Runtime/common/misc/RTAssertMsg2AddWeakV.cpp
SRC_CC += Runtime/common/misc/RTAssertMsg2Weak.cpp
SRC_CC += Runtime/common/misc/RTAssertMsg2WeakV.cpp
SRC_CC += Runtime/common/misc/RTMemWipeThoroughly.cpp
SRC_CC += Runtime/common/path/comparepaths.cpp
SRC_CC += Runtime/common/path/RTPathAbsDup.cpp
SRC_CC += Runtime/common/path/RTPathAbsEx.cpp
SRC_CC += Runtime/common/path/RTPathAppendEx.cpp
SRC_CC += Runtime/common/path/RTPathCalcRelative.cpp
SRC_CC += Runtime/common/path/RTPathExt.cpp
SRC_CC += Runtime/common/path/RTPathFilename.cpp
SRC_CC += Runtime/common/path/RTPathHasPath.cpp
SRC_CC += Runtime/common/path/RTPathJoinA.cpp
SRC_CC += Runtime/common/path/RTPathJoinEx.cpp
SRC_CC += Runtime/common/path/RTPathParse.cpp
SRC_CC += Runtime/common/path/RTPathParseSimple.cpp
SRC_CC += Runtime/common/path/RTPathRealDup.cpp
SRC_CC += Runtime/common/path/RTPathStartsWithRoot.cpp
SRC_CC += Runtime/common/path/RTPathStripExt.cpp
SRC_CC += Runtime/common/path/RTPathStripFilename.cpp
SRC_CC += Runtime/common/path/RTPathStripTrailingSlash.cpp
SRC_CC += Runtime/common/path/rtPathVolumeSpecLen.cpp
SRC_CC += Runtime/common/path/rtPathRootSpecLen.cpp
SRC_CC += Runtime/common/rand/rand.cpp
SRC_CC += Runtime/common/rand/randadv.cpp
SRC_CC += Runtime/common/rand/randparkmiller.cpp
SRC_CC += Runtime/common/string/base64.cpp
SRC_CC += Runtime/common/string/RTStrCmp.cpp
SRC_CC += Runtime/common/string/RTStrCopy.cpp
SRC_CC += Runtime/common/string/RTStrCopyEx.cpp
SRC_CC += Runtime/common/string/RTStrNCmp.cpp
SRC_CC += Runtime/common/string/RTStrNLen.cpp
SRC_CC += Runtime/common/string/RTStrPrintHexBytes.cpp
SRC_CC += Runtime/common/string/simplepattern.cpp
SRC_CC += Runtime/common/string/straprintf.cpp
SRC_CC += Runtime/common/string/strformat.cpp
SRC_CC += Runtime/common/string/strformatrt.cpp
SRC_CC += Runtime/common/string/strformattype.cpp
SRC_CC += Runtime/common/string/stringalloc.cpp
SRC_CC += Runtime/common/string/strprintf.cpp
SRC_CC += Runtime/common/string/strspace.cpp
SRC_CC += Runtime/common/string/strstrip.cpp
SRC_CC += Runtime/common/string/strtonum.cpp
SRC_CC += Runtime/common/string/unidata-lower.cpp
SRC_CC += Runtime/common/string/unidata-upper.cpp
SRC_CC += Runtime/common/string/utf-8.cpp
SRC_CC += Runtime/common/string/utf-8-case.cpp
SRC_CC += Runtime/common/string/utf-16.cpp
SRC_CC += Runtime/common/string/utf-16-case.cpp
SRC_CC += Runtime/common/table/avlpv.cpp
SRC_CC += Runtime/common/table/avlroioport.cpp
SRC_CC += Runtime/common/table/avlrogcphys.cpp
SRC_CC += Runtime/common/table/avlul.cpp
SRC_CC += Runtime/common/time/time.cpp
SRC_CC += Runtime/common/time/timeprog.cpp
SRC_CC += Runtime/common/time/timesup.cpp
SRC_CC += Runtime/common/time/timesupref.cpp
SRC_CC += Runtime/common/vfs/vfsbase.cpp
SRC_CC += Runtime/common/zip/zip.cpp
SRC_CC += Runtime/r3/alloc.cpp
SRC_CC += Runtime/r3/dir.cpp
SRC_CC += Runtime/r3/fileio.cpp
SRC_CC += Runtime/r3/fs.cpp
SRC_CC += Runtime/r3/path.cpp
SRC_CC += Runtime/r3/generic/semspinmutex-r3-generic.cpp
SRC_CC += Runtime/r3/posix/dir-posix.cpp
SRC_CC += Runtime/r3/posix/env-posix.cpp
SRC_CC += Runtime/r3/posix/fileio-posix.cpp
SRC_CC += Runtime/r3/posix/fileio2-posix.cpp
SRC_CC += Runtime/r3/posix/fs-posix.cpp
SRC_CC += Runtime/r3/posix/fs2-posix.cpp
SRC_CC += Runtime/r3/posix/fs3-posix.cpp
SRC_CC += Runtime/r3/posix/path-posix.cpp
SRC_CC += Runtime/r3/posix/path2-posix.cpp
SRC_CC += Runtime/r3/posix/pipe-posix.cpp
SRC_CC += Runtime/r3/posix/RTTimeNow-posix.cpp
SRC_CC += Runtime/r3/posix/semeventmulti-posix.cpp
SRC_CC += Runtime/r3/posix/semevent-posix.cpp
SRC_CC += Runtime/r3/posix/semmutex-posix.cpp
SRC_CC += Runtime/r3/posix/thread2-posix.cpp
SRC_CC += Runtime/r3/posix/thread-posix.cpp
SRC_CC += Runtime/r3/posix/time-posix.cpp
SRC_CC += Runtime/r3/posix/tls-posix.cpp
SRC_CC += Runtime/r3/posix/utf8-posix.cpp
SRC_CC += Runtime/r3/process.cpp
SRC_CC += Runtime/r3/stream.cpp
SRC_CC += Runtime/VBox/log-vbox.cpp
SRC_S  += Runtime/common/asm/ASMAtomicCmpXchgU64.asm
SRC_S  += Runtime/common/asm/ASMAtomicReadU64.asm
SRC_S  += Runtime/common/asm/ASMAtomicUoReadU64.as
SRC_S  += Runtime/common/asm/ASMAtomicXchgU64.asm
SRC_S  += Runtime/common/asm/ASMCpuIdExSlow.asm

SRC_CC += Runtime/common/err/errmsg.cpp
Runtime/common/err/errmsg.o: errmsgdata.h

errmsgdata.h: $(VIRTUALBOX_DIR)/include/iprt/err.h \
              $(VIRTUALBOX_DIR)/include/VBox/err.h
	$(MSG_CONVERT)$@
	$(VERBOSE)sed -f $(VBOX_DIR)/Runtime/common/err/errmsg.sed $^ > $@

SRC_CC += Runtime/common/err/errmsgxpcom.cpp
Runtime/common/err/errmsgxpcom.o: errmsgvboxcomdata.h

errmsgvboxcomdata.h:
	touch $@
