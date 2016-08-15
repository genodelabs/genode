include $(REP_DIR)/lib/mk/virtualbox5-common.inc

RECOMPILER_DIR = $(VIRTUALBOX_DIR)/src/recompiler

RECOMPILER_SRC_C = cpu-exec.c cutils.c exec.c host-utils.c tcg-runtime.c \
				   translate-all.c VBoxRecompiler.c \
                   tcg/tcg-dyngen.c tcg/tcg.c \
                   fpu/softfloat-native.c \
                   target-i386/helper.c target-i386/op_helper.c \
                   target-i386/translate.c

SRC_C = $(addprefix recompiler/,$(RECOMPILER_SRC_C))

INC_DIR += $(VBOX_DIR)/VMM/include
INC_DIR += $(RECOMPILER_DIR)/Sun/crt
INC_DIR += $(RECOMPILER_DIR)/Sun
INC_DIR += $(RECOMPILER_DIR)/target-i386
INC_DIR += $(RECOMPILER_DIR)/tcg
INC_DIR += $(RECOMPILER_DIR)/tcg/i386
INC_DIR += $(RECOMPILER_DIR)/fpu
INC_DIR += $(RECOMPILER_DIR)

CC_OPT += -DIN_REM_R3 \
          -DREM_INCLUDE_CPU_H -DNEED_CPU_H -DLOG_USE_C99 -D_GNU_SOURCE

CC_WARN += -Wno-unused-but-set-variable]

vpath %.cpp $(VIRTUALBOX_DIR)/src
vpath %.c   $(VIRTUALBOX_DIR)/src
