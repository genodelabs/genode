LIBS          = libc
SHARED_LIB    = yes
JDK_BASE      = $(call select_from_ports,jdk)/src/app/jdk/jdk/src/java.base
JDK_GENERATED = $(call select_from_ports,jdk_generated)/src/app/jdk

SRC_C = fs/UnixNativeDispatcher.c \
        fs/UnixCopyFile.c \
        MappedByteBuffer.c \
        ch/UnixAsynchronousServerSocketChannelImpl.c \
        ch/FileKey.c \
        ch/UnixAsynchronousSocketChannelImpl.c \
        ch/SocketDispatcher.c \
        ch/NativeThread.c \
        ch/DatagramChannelImpl.c \
        ch/FileChannelImpl.c \
        ch/PollArrayWrapper.c \
        ch/InheritedChannel.c \
        ch/Net.c \
        ch/FileDispatcherImpl.c \
        ch/IOUtil.c \
        ch/DatagramDispatcher.c \
        ch/ServerSocketChannelImpl.c \
        ch/SocketChannelImpl.c


INC_DIR += $(JDK_GENERATED)/include/java.base \
           $(JDK_BASE)/share/native/include \
           $(JDK_BASE)/share/native/libjava \
           $(JDK_BASE)/share/native/libnet \
           $(JDK_BASE)/share/native/libnio \
           $(JDK_BASE)/share/native/libnio/ch \
           $(JDK_BASE)/unix/native/include \
           $(JDK_BASE)/unix/native/libjava \
           $(JDK_BASE)/unix/native/libnet \
           $(JDK_BASE)/unix/native/libnio

CC_C_OPT = -D_ALLBSD_SOURCE -include netinet/in.h

CC_OPT_ch/Net += -DIPV6_ADD_MEMBERSHIP=IPV6_JOIN_GROUP -DIPV6_DROP_MEMBERSHIP=IPV6_LEAVE_GROUP
CC_OPT_net_util_md += -include sys/socket.h

vpath %.c $(JDK_BASE)/unix/native/libnio
