REQUIRES   = x86 32bit
SHARED_LIB = yes
DIR = $(REP_DIR)/src/lib/ldso

INC_DIR += $(DIR)/contrib/i386 \
           $(DIR)/include/libc/libc-i386 \
           $(DIR)/include/x86_32

vpath %    $(DIR)/contrib/i386
vpath %    $(DIR)/x86_32

include $(DIR)/target.inc
