LIBS          = libc
SHARED_LIB    = yes
JDK_BASE      = $(call select_from_ports,jdk)/src/app/jdk/jdk/src/java.base
JDK_GENERATED = $(call select_from_ports,jdk_generated)/src/app/jdk

CC_CXX_WARN_STRICT =

SRC_CC = endian.cpp \
         imageDecompressor.cpp \
         imageFile.cpp \
         jimage.cpp \
         NativeImageBuffer.cpp \
         osSupport_unix.cpp

INC_DIR += $(JDK_BASE)/share/native/include \
           $(JDK_BASE)/share/native/libjava \
           $(JDK_BASE)/share/native/libjimage \
           $(JDK_BASE)/unix/native/include \
           $(JDK_BASE)/unix/native/libjava \
           $(JDK_GENERATED)/include/java.base

vpath %.cpp $(JDK_BASE)/share/native/libjimage
vpath %.cpp $(JDK_BASE)/unix/native/libjimage
