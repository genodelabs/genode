LIBS          = libc
SHARED_LIB    = yes
JDK_BASE      = $(call select_from_ports,jdk)/src/app/jdk/jdk/src/java.base
JDK_GENERATED = $(call select_from_ports,jdk_generated)/src/app/jdk
JAVA_BASE     = $(JDK_BASE)/share/native/libjava
VERIFY_BASE   = $(JDK_BASE)/share/native/libverify

CC_OLEVEL = -O0

SRC_C  = $(notdir $(wildcard $(JAVA_BASE)/*.c))
SRC_C += unix/native/libjava/canonicalize_md.c \
         unix/native/libjava/FileDescriptor_md.c \
         unix/native/libjava/FileInputStream_md.c \
         unix/native/libjava/FileOutputStream_md.c \
         unix/native/libjava/io_util_md.c \
         unix/native/libjava/java_props_md.c \
         unix/native/libjava/jdk_util_md.c \
         unix/native/libjava/jni_util_md.c \
         unix/native/libjava/ProcessEnvironment_md.c \
         unix/native/libjava/RandomAccessFile_md.c \
         unix/native/libjava/TimeZone_md.c \
         unix/native/libjava/UnixFileSystem_md.c

SRC_C += check_format.c check_code.c
SRC_C += math_genode.c


include $(REP_DIR)/lib/mk/jdk_version.inc

CC_C_OPT = $(JDK_VERSION)
CC_C_OPT += -D_ALLBSD_SOURCE -D__GENODE__
CC_OPT_unix/native/libjava/TimeZone_md = -D__linux__

INC_DIR += $(REP_DIR)/src/app/jdk/lib/include \
           $(JDK_GENERATED)/include/java.base \
           $(JDK_BASE)/share/native/include \
           $(JDK_BASE)/share/native/libfdlibm \
           $(JDK_BASE)/share/native/libjava \
           $(JDK_BASE)/unix/native/include \
           $(JDK_BASE)/unix/native/libjava

vpath %.c $(JAVA_BASE)
vpath %.c $(VERIFY_BASE)
vpath %.c $(JDK_BASE)
vpath %.c $(REP_DIR)/src/app/jdk/lib/java

# vi: set ft=make :
