LIBS = libc

include $(REP_DIR)/lib/mk/mesa-common.inc

CC_OPT  += -DGFX_VERx10=120
include $(REP_DIR)/lib/mk/iris_gen.inc

