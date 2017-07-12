LIBS       = libc zlib
SHARED_LIB = yes
JDK_BASE   = $(call select_from_ports,jdk)/src/app/jdk/jdk/src/java.base

CC_OLEVEL = -O0

SRC_C  = args.c \
         java.c \
         java_md_common.c \
         java_md_solinux.c \
         jli_util.c \
         parse_manifest.c \
         splashscreen_stubs.c \
         wildcard.c


INC_DIR += $(JDK_BASE)/share/native/include \
           $(JDK_BASE)/share/native/libjli \
           $(JDK_BASE)/unix/native/include \
           $(JDK_BASE)/unix/native/libjli

CC_C_OPT = -D__linux__ -D__GENODE__ -Dlseek64=lseek

vpath %.c $(JDK_BASE)/share/native/libjli
vpath %.c $(JDK_BASE)/unix/native/libjli

