JDK_BASE = $(call select_from_ports,jdk)/src/app/jdk/jdk/src/java.base
INC_DIR += $(JDK_BASE)/share/native/libjli \
           $(JDK_BASE)/share/native/include \
           $(JDK_BASE)/unix/native/include \
           $(JDK_BASE)/unix/native/libjli


