TARGET   = java
SRC_C    = main.c
LIBS     = jli java jvm libc posix libc_pipe

CC_C_OPT = -DVERSION_STRING='"9-genode.openjdk"' -D__GENODE__

JDK_PATH = $(call select_from_ports,jdk)/src/app/jdk
vpath main.c $(JDK_PATH)/jdk/src/java.base/share/native/launcher
