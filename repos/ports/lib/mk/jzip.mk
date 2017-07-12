LIBS          = libc zlib
SHARED_LIB    = yes
JDK_BASE      = $(call select_from_ports,jdk)/src/app/jdk/jdk/src/java.base
JDK_GENERATED = $(call select_from_ports,jdk_generated)/src/app/jdk

SRC_C = Adler32.c CRC32.c Deflater.c Inflater.c zip_util.c

INC_DIR += $(JDK_BASE)/share/native/include \
           $(JDK_BASE)/share/native/libjava \
           $(JDK_BASE)/unix/native/include \
           $(JDK_BASE)/unix/native/libjava \
           $(JDK_GENERATED)/include/java.base

CC_C_OPT = -D_ALLBSD_SOURCE

vpath %.c $(JDK_BASE)/share/native/libzip
