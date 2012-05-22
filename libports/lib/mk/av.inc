CC_C_OPT += -DHAVE_AV_CONFIG_H
CC_C_OPT += -std=c99 -DPIC

INC_DIR += $(REP_DIR)/src/lib/libav

include $(REP_DIR)/src/lib/libav/config.mak

SRC_C += $(OBJS:.o=.c)
SRC_C += $(OBJS-yes:.o=.c)

LIBS  += libc libm

SHARED_LIB = yes
