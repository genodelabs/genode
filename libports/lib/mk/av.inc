CC_C_OPT += -DHAVE_AV_CONFIG_H
CC_C_OPT += -std=c99 -DPIC

# The third-party sources induce a lot of "assignment discards ‘const’
# qualifier" and "passing argument from incompatible pointer type" warnings.
# Therefore, switch off _all_ warnings.
CC_WARN += -w

# Some source files do not build with -O0 due to conflicting register
# allocation and inline assembler constrains. We enforce optimization for these
# files.
CC_OPT_h264_cabac = -O2
CC_OPT_motion_est = -O2

INC_DIR += $(REP_DIR)/src/lib/libav

include $(REP_DIR)/src/lib/libav/config.mak

SRC_C += $(OBJS:.o=.c)
SRC_C += $(OBJS-yes:.o=.c)

LIBS  += libc libm

SHARED_LIB = yes
