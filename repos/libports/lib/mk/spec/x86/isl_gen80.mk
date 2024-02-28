LIBS = libc

include $(REP_DIR)/lib/mk/mesa-common.inc

CC_OPT  += -DGFX_VERx10=80
include $(REP_DIR)/lib/mk/isl_gen.inc
