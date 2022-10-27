include $(REP_DIR)/lib/mk/virtualbox6-common.inc

LIBICONV_DIR := $(call select_from_ports,libiconv)

INC_DIR += $(VBOX_DIR)/Runtime/include

INC_DIR += $(VIRTUALBOX_DIR)/src/libs/liblzf-3.4
INC_DIR += $(VIRTUALBOX_DIR)/src/libs/zlib-1.2.13
INC_DIR += $(LIBICONV_DIR)/include/iconv
INC_DIR += $(REP_DIR)/src/virtualbox6/include/libc

LIBS += stdcxx

CC_WARN += -Wno-unused-variable

all_cpp_files_of_sub_dir = \
  $(addprefix $1, $(notdir $(wildcard $(VBOX_DIR)/$1*.cpp)))

SRC_CC += $(call all_cpp_files_of_sub_dir,Runtime/common/alloc/)
SRC_CC += $(call all_cpp_files_of_sub_dir,Runtime/common/err/)
SRC_CC += $(call all_cpp_files_of_sub_dir,Runtime/common/log/)
SRC_CC += $(call all_cpp_files_of_sub_dir,Runtime/common/misc/)
SRC_CC += $(call all_cpp_files_of_sub_dir,Runtime/common/path/)
SRC_CC += $(call all_cpp_files_of_sub_dir,Runtime/common/rand/)
SRC_CC += $(call all_cpp_files_of_sub_dir,Runtime/common/string/)
SRC_CC += $(call all_cpp_files_of_sub_dir,Runtime/common/table/)
SRC_CC += $(call all_cpp_files_of_sub_dir,Runtime/generic/)
SRC_CC += $(call all_cpp_files_of_sub_dir,Runtime/r3/)

SRC_CC += Runtime/VBox/log-vbox.cpp
SRC_CC += Runtime/common/checksum/alt-md5.cpp
SRC_CC += Runtime/common/checksum/alt-sha512.cpp
SRC_CC += Runtime/common/checksum/crc16ccitt.cpp
SRC_CC += Runtime/common/checksum/crc32.cpp
SRC_CC += Runtime/common/checksum/crc32c.cpp
SRC_CC += Runtime/common/checksum/ipv4.cpp
SRC_CC += Runtime/common/checksum/ipv6.cpp
SRC_CC += Runtime/common/checksum/sha512str.cpp
SRC_CC += Runtime/common/dbg/dbgstackdumpself.cpp
SRC_CC += Runtime/common/fs/isovfs.cpp
SRC_CC += Runtime/common/ldr/ldr.cpp
SRC_CC += Runtime/common/ldr/ldrEx.cpp
SRC_CC += Runtime/common/ldr/ldrFile.cpp
SRC_CC += Runtime/common/ldr/ldrNative.cpp
SRC_CC += Runtime/common/net/macstr.cpp
SRC_CC += Runtime/common/net/netaddrstr2.cpp
SRC_CC += Runtime/common/sort/shellsort.cpp
SRC_CC += Runtime/common/time/time.cpp
SRC_CC += Runtime/common/time/timeprog.cpp
SRC_CC += Runtime/common/time/timesup.cpp
SRC_CC += Runtime/common/time/timesupref.cpp
SRC_CC += Runtime/common/vfs/vfsbase.cpp
SRC_CC += Runtime/common/vfs/vfschain.cpp
SRC_CC += Runtime/common/vfs/vfsprogress.cpp
SRC_CC += Runtime/common/vfs/vfsstddir.cpp
SRC_CC += Runtime/common/vfs/vfsstdfile.cpp
SRC_CC += Runtime/common/zip/zip.cpp
SRC_CC += Runtime/r3/freebsd/systemmem-freebsd.cpp
SRC_CC += Runtime/r3/generic/dirrel-r3-generic.cpp
SRC_CC += Runtime/r3/generic/semspinmutex-r3-generic.cpp
SRC_CC += Runtime/r3/posix/RTPathUserHome-posix.cpp
SRC_CC += Runtime/r3/posix/RTTimeNow-posix.cpp
SRC_CC += Runtime/r3/posix/allocex-r3-posix.cpp
SRC_CC += Runtime/r3/posix/dir-posix.cpp
SRC_CC += Runtime/r3/posix/env-posix.cpp
SRC_CC += Runtime/r3/posix/fileaio-posix.cpp
SRC_CC += Runtime/r3/posix/fileio-posix.cpp
SRC_CC += Runtime/r3/posix/fileio2-posix.cpp
SRC_CC += Runtime/r3/posix/fs-posix.cpp
SRC_CC += Runtime/r3/posix/fs2-posix.cpp
SRC_CC += Runtime/r3/posix/fs3-posix.cpp
SRC_CC += Runtime/r3/posix/ldrNative-posix.cpp
SRC_CC += Runtime/r3/posix/path-posix.cpp
SRC_CC += Runtime/r3/posix/path2-posix.cpp
SRC_CC += Runtime/r3/posix/pipe-posix.cpp
SRC_CC += Runtime/r3/posix/process-posix.cpp
SRC_CC += Runtime/r3/posix/rtmempage-exec-mmap-posix.cpp
SRC_CC += Runtime/r3/posix/RTMemProtect-posix.cpp
SRC_CC += Runtime/r3/posix/semevent-posix.cpp
SRC_CC += Runtime/r3/posix/semeventmulti-posix.cpp
SRC_CC += Runtime/r3/posix/semmutex-posix.cpp
SRC_CC += Runtime/r3/posix/serialport-posix.cpp
SRC_CC += Runtime/r3/posix/symlink-posix.cpp
SRC_CC += Runtime/r3/posix/thread-posix.cpp
SRC_CC += Runtime/r3/posix/thread2-posix.cpp
SRC_CC += Runtime/r3/posix/time-posix.cpp
SRC_CC += Runtime/r3/posix/tls-posix.cpp
SRC_CC += Runtime/r3/posix/utf8-posix.cpp
SRC_S  += Runtime/common/asm/ASMAtomicCmpXchgExU64.asm
SRC_S  += Runtime/common/asm/ASMAtomicCmpXchgU64.asm
SRC_S  += Runtime/common/asm/ASMAtomicReadU64.asm
SRC_S  += Runtime/common/asm/ASMAtomicUoReadU64.as
SRC_S  += Runtime/common/asm/ASMAtomicXchgU64.asm
SRC_S  += Runtime/common/asm/ASMCpuIdExSlow.asm
SRC_S  += Runtime/common/asm/ASMGetXcr0.asm
SRC_S  += Runtime/common/asm/ASMFxSave.asm
SRC_S  += Runtime/common/asm/ASMMemFirstMismatchingU8.asm
SRC_S  += Runtime/common/dbg/dbgstackdumpself-amd64-x86.asm

FILTERED_OUT_SRC_CC += Runtime/common/misc/RTSystemIsInsideVM-amd64-x86.cpp
FILTERED_OUT_SRC_CC += Runtime/common/misc/s3.cpp
FILTERED_OUT_SRC_CC += Runtime/common/string/ministring.cpp
FILTERED_OUT_SRC_CC += Runtime/generic/fs-stubs-generic.cpp
FILTERED_OUT_SRC_CC += Runtime/generic/http-curl.cpp
FILTERED_OUT_SRC_CC += Runtime/generic/mppresent-generic-online.cpp
FILTERED_OUT_SRC_CC += Runtime/generic/RTDirExists-generic.cpp
FILTERED_OUT_SRC_CC += Runtime/generic/RTFileExists-generic.cpp
FILTERED_OUT_SRC_CC += Runtime/generic/RTLogDefaultInit-generic.cpp
FILTERED_OUT_SRC_CC += Runtime/generic/RTLogWriteStdErr-generic.cpp
FILTERED_OUT_SRC_CC += Runtime/generic/RTLogWriteStdOut-generic.cpp
FILTERED_OUT_SRC_CC += Runtime/generic/RTMpGetDescription-generic-stub.cpp
FILTERED_OUT_SRC_CC += Runtime/generic/RTMpOnPair-generic.cpp
FILTERED_OUT_SRC_CC += Runtime/generic/RTProcessQueryUsernameA-generic.cpp
FILTERED_OUT_SRC_CC += Runtime/generic/RTSemEventMultiWait-2-ex-generic.cpp
FILTERED_OUT_SRC_CC += Runtime/generic/RTSemEventWait-2-ex-generic.cpp
FILTERED_OUT_SRC_CC += Runtime/generic/RTSemEventWait-generic.cpp
FILTERED_OUT_SRC_CC += Runtime/generic/RTSemEventWaitNoResume-2-ex-generic.cpp
FILTERED_OUT_SRC_CC += Runtime/generic/RTSemMutexRequestDebug-generic.cpp
FILTERED_OUT_SRC_CC += Runtime/generic/RTSemMutexRequest-generic.cpp
FILTERED_OUT_SRC_CC += Runtime/generic/semrw-lockless-generic.cpp
FILTERED_OUT_SRC_CC += Runtime/generic/strcache-stubs-generic.cpp
FILTERED_OUT_SRC_CC += Runtime/generic/tls-generic.cpp
FILTERED_OUT_SRC_CC += Runtime/r3/xml.cpp
FILTERED_OUT_SRC_CC += Runtime/r3/alloc-ef.cpp
FILTERED_OUT_SRC_CC += Runtime/r3/alloc-ef-cpp.cpp
FILTERED_OUT_SRC_CC += Runtime/r3/memsafer-r3.cpp

# avoid static allocation of 1 MiB array 'g_aCPInfo'
FILTERED_OUT_SRC_CC += Runtime/common/string/uniread.cpp

SRC_CC := $(filter-out $(FILTERED_OUT_SRC_CC), $(SRC_CC))

Runtime/common/err/errmsg.o: errmsgdata.h

errmsgdata.h: $(VIRTUALBOX_DIR)/include/iprt/err.h \
              $(VIRTUALBOX_DIR)/include/VBox/err.h
	$(MSG_CONVERT)$@
	$(VERBOSE)sed -f $(VBOX_DIR)/Runtime/common/err/errmsg.sed $^ > $@

CC_CXX_WARN_STRICT =
