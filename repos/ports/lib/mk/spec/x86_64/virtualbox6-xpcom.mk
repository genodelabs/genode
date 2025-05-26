include $(REP_DIR)/lib/mk/virtualbox6-common.inc

XPCOM_DIR := $(VIRTUALBOX_DIR)/src/libs/xpcom18a4

all_cpp_files_of_sub_dir = \
  $(addprefix $1, $(notdir $(wildcard $(XPCOM_DIR)/$1*.cpp)))

LIBS  += stdcxx

SRC_CC += $(call all_cpp_files_of_sub_dir,xpcom/ds/)
SRC_CC += $(call all_cpp_files_of_sub_dir,xpcom/base/)
SRC_CC += Main/glue/AutoLock.cpp
SRC_CC += Main/glue/com.cpp
SRC_CC += Main/glue/ErrorInfo.cpp
SRC_CC += Main/glue/EventQueue.cpp
SRC_CC += Main/glue/NativeEventQueue.cpp
SRC_CC += Main/glue/GetVBoxUserHomeDirectory.cpp
SRC_CC += Main/glue/string.cpp
SRC_CC += Main/glue/xpcom/helpers.cpp
SRC_CC += xpcom/base/nsID.cpp
SRC_CC += xpcom/build/nsXPComInit.cpp
SRC_CC += xpcom/components/nsCategoryManager.cpp
SRC_CC += xpcom/components/nsComponentManager.cpp
SRC_CC += xpcom/components/nsNativeComponentLoader.cpp
SRC_CC += xpcom/components/nsServiceManagerObsolete.cpp
SRC_CC += xpcom/components/nsStaticComponentLoader.cpp
SRC_CC += xpcom/components/xcDll.cpp
SRC_CC += xpcom/glue/nsCOMPtr.cpp
SRC_CC += xpcom/glue/nsComponentManagerUtils.cpp
SRC_CC += xpcom/glue/nsGenericFactory.cpp
SRC_CC += xpcom/glue/nsMemory.cpp
SRC_CC += xpcom/glue/nsWeakReference.cpp
SRC_CC += xpcom/io/SpecialSystemDirectory.cpp
SRC_CC += xpcom/io/nsAppFileLocationProvider.cpp
SRC_CC += xpcom/io/nsBinaryStream.cpp
SRC_CC += xpcom/io/nsDirectoryService.cpp
SRC_CC += xpcom/io/nsLocalFileCommon.cpp
SRC_CC += xpcom/io/nsLocalFileUnix.cpp
SRC_CC += xpcom/io/nsNativeCharsetUtils.cpp
SRC_CC += xpcom/io/nsSegmentedBuffer.cpp
SRC_CC += xpcom/io/nsStorageStream.cpp
SRC_CC += xpcom/string/src/nsAString.cpp
SRC_CC += xpcom/string/src/nsDependentSubstring.cpp
SRC_CC += xpcom/string/src/nsObsoleteAStringThunk.cpp
SRC_CC += xpcom/string/src/nsPrintfCString.cpp
SRC_CC += xpcom/string/src/nsPromiseFlatString.cpp
SRC_CC += xpcom/string/src/nsReadableUtils.cpp
SRC_CC += xpcom/string/src/nsString.cpp
SRC_CC += xpcom/string/src/nsStringComparator.cpp
SRC_CC += xpcom/string/src/nsStringObsolete.cpp
SRC_CC += xpcom/string/src/nsSubstring.cpp
SRC_CC += xpcom/string/src/nsSubstringTuple.cpp
SRC_CC += xpcom/threads/TimerThread.cpp
SRC_CC += xpcom/threads/nsAutoLock.cpp
SRC_CC += xpcom/threads/nsEnvironment.cpp
SRC_CC += xpcom/threads/nsEventQueue.cpp
SRC_CC += xpcom/threads/nsEventQueueService.cpp
SRC_CC += xpcom/threads/nsProcessCommon.cpp
SRC_CC += xpcom/threads/nsThread.cpp
SRC_CC += xpcom/threads/nsTimerImpl.cpp

FILTERED_OUT_SRC_CC += xpcom/base/nsStackFrameWin.cpp
FILTERED_OUT_SRC_CC += xpcom/ds/nsPersistentProperties.cpp
FILTERED_OUT_SRC_CC += xpcom/ds/nsTextFormatter.cpp

SRC_CC := $(filter-out $(FILTERED_OUT_SRC_CC), $(SRC_CC))

SRC_C += nsprpub/lib/ds/plarena.c
SRC_C += nsprpub/lib/libc/src/strccmp.c
SRC_C += nsprpub/lib/libc/src/strccmp.c
SRC_C += nsprpub/lib/libc/src/strcmp.c
SRC_C += nsprpub/lib/libc/src/strlen.c
SRC_C += nsprpub/lib/libc/src/strdup.c
SRC_C += nsprpub/pr/src/io/prfdcach.c
SRC_C += nsprpub/pr/src/io/priometh.c
SRC_C += nsprpub/pr/src/io/prlog.c
SRC_C += nsprpub/pr/src/io/prmmap.c
SRC_C += nsprpub/pr/src/io/prmwait.c
SRC_C += nsprpub/pr/src/io/prprf.c
SRC_C += nsprpub/pr/src/io/prstdio.c
SRC_C += nsprpub/pr/src/io/prscanf.c
SRC_C += nsprpub/pr/src/malloc/prmem.c
SRC_C += nsprpub/pr/src/md/prosdep.c
SRC_C += nsprpub/pr/src/md/unix/unix.c
SRC_C += nsprpub/pr/src/md/unix/unix_errors.c
SRC_C += nsprpub/pr/src/misc/pratom.c
SRC_C += nsprpub/pr/src/misc/prdtoa.c
SRC_C += nsprpub/pr/src/misc/prenv.c
SRC_C += nsprpub/pr/src/misc/prerr.c
SRC_C += nsprpub/pr/src/misc/prerror.c
SRC_C += nsprpub/pr/src/misc/prerrortable.c
SRC_C += nsprpub/pr/src/misc/prinit.c
SRC_C += nsprpub/pr/src/misc/prinrval.c
SRC_C += nsprpub/pr/src/misc/prlog2.c
SRC_C += nsprpub/pr/src/misc/prlong.c
SRC_C += nsprpub/pr/src/misc/prnetdb.c
SRC_C += nsprpub/pr/src/pthreads/ptio.c
SRC_C += nsprpub/pr/src/pthreads/ptsynch.c
SRC_C += nsprpub/pr/src/pthreads/ptthread.c
SRC_C += nsprpub/pr/src/threads/prcmon.c
SRC_C += nsprpub/pr/src/threads/prrwlock.c
SRC_C += nsprpub/pr/src/threads/prtpd.c
SRC_C += xpcom/ds/pldhash.c
SRC_C += xpcom/threads/plevent.c

vpath %.cpp $(XPCOM_DIR)
vpath %.c   $(XPCOM_DIR)

INC_DIR += $(REP_DIR)/src/virtualbox6
SRC_CC  += xpcom_stubs.cc
vpath xpcom_stubs.cc $(REP_DIR)/src/virtualbox6

INC_DIR += $(XPCOM_DIR)
INC_DIR += $(XPCOM_DIR)/nsprpub/pr/include/private
INC_DIR += $(XPCOM_DIR)/nsprpub/pr/include/md
INC_DIR += $(XPCOM_DIR)/xpcom/build
INC_DIR += $(XPCOM_DIR)/xpcom/ds
INC_DIR += $(XPCOM_DIR)/xpcom/io
INC_DIR += $(XPCOM_DIR)/xpcom/base
INC_DIR += $(XPCOM_DIR)/xpcom/threads
INC_DIR += $(XPCOM_DIR)/xpcom/proxy/src
INC_DIR += $(XPCOM_DIR)/xpcom/components
INC_DIR += $(XPCOM_DIR)/ipc/ipcd/client/src
INC_DIR += $(XPCOM_DIR)/ipc/ipcd/shared/src
INC_DIR += $(XPCOM_DIR)/ipc/ipcd/extensions/lock/src
INC_DIR += $(XPCOM_DIR)/ipc/ipcd/extensions/transmngr/src
INC_DIR += $(XPCOM_DIR)/ipc/ipcd/extensions/transmngr/common
INC_DIR += $(XPCOM_DIR)/ipc/ipcd/extensions/dconnect/src

CC_OPT += -D_PR_PTHREADS
CC_OPT += -DMOZ_DLL_SUFFIX= '-DMOZ_USER_DIR=""'

CC_OPT_xpcom/ds/nsQuickSort := -Dregister=

CC_OPT_nsprpub/pr/src/md/unix/unix.c      := -Wno-incompatible-pointer-types
CC_OPT_nsprpub/pr/src/pthreads/ptthread.c := -Wno-incompatible-pointer-types

CC_WARN :=
CC_C_WARN := -Wno-pointer-to-int-cast
CC_CXX_WARN := -Wno-stringop-overflow -Wno-literal-suffix -Wno-invalid-offsetof

CC_CXX_WARN_STRICT =
