#
# C Library including string, locale
#
LIBS   = libc-string libc-locale libc-stdlib libc-stdio libc-gen libc-gdtoa \
         libc-inet libc-stdtime libc-regex libc-compat libc-setjmp libc-mem \
         libc-resolv libc-isc libc-nameser libc-net libc-rpc libc-tzcode \
         libc-libkern

LIBS  += base vfs

#
# Back end
#
SRC_CC = atexit.cc dummies.cc rlimit.cc sysctl.cc \
         issetugid.cc errno.cc gai_strerror.cc time.cc \
         malloc.cc progname.cc fd_alloc.cc file_operations.cc \
         plugin.cc plugin_registry.cc select.cc exit.cc environ.cc sleep.cc \
         pread_pwrite.cc readv_writev.cc poll.cc \
         vfs_plugin.cc dynamic_linker.cc signal.cc \
         socket_operations.cc socket_fs_plugin.cc syscall.cc \
         getpwent.cc getrandom.cc fork.cc execve.cc kernel.cc component.cc \
         genode.cc spinlock.cc

#
# Pthreads
#
SRC_CC += semaphore.cc rwlock.cc \
          pthread.cc pthread_create.cc

#
# FreeBSD headers use the C99 restrict keyword
#
CXX_DEF += -Drestrict=__restrict

#
# Extra include path for internal dummies
#
CC_OPT_dummies += -I$(LIBC_DIR)/sys

INC_DIR += $(REP_DIR)/src/lib/libc
INC_DIR += $(REP_DIR)/src/lib/libc/include

# needed for base/internal/unmanaged_singleton.h
INC_DIR += $(BASE_DIR)/src/include
INC_DIR += $(BASE_DIR)/sys

#
# Files from string library that are not included in libc-raw_string because
# they depend on the locale library.
#
SRC_C += strcoll.c strxfrm.c wcscoll.c wcsxfrm.c

include $(REP_DIR)/lib/mk/libc-common.inc

vpath % $(REP_DIR)/src/lib/libc
vpath % $(LIBC_DIR)/lib/libc/string

#
# Shared library, for libc we need symbol versioning
#
SHARED_LIB  = yes
LD_OPT     += --version-script=$(LIBC_DIR)/lib/libc/Versions.def
