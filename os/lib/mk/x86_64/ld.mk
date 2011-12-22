REQUIRES   = x86 64bit
SHARED_LIB = yes
DIR        = $(REP_DIR)/src/lib/ldso

INC_DIR += $(DIR)/contrib/amd64 \
           $(DIR)/include/libc/libc-amd64 \
           $(DIR)/include/x86_64

D_OPTS = __ELF_WORD_SIZE=64


vpath %    $(DIR)/contrib/amd64

include $(DIR)/target.inc
