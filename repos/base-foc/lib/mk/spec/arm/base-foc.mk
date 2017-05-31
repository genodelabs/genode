# override default stack-area location
INC_DIR += $(REP_DIR)/src/include/spec/arm

LIBS += timeout-arm

include $(REP_DIR)/lib/mk/base-foc.inc
