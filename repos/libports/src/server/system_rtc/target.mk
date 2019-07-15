TARGET := system_rtc
SRC_CC := main.cc
LIBS   := base

# musl-libc contrib sources
MUSL_TM  = $(REP_DIR)/src/lib/musl_tm
SRC_C    = secs_to_tm.c tm_to_secs.c
INC_DIR += $(MUSL_TM)

vpath %.c $(MUSL_TM)
