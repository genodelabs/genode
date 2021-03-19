LIBS += libdrm iris

CC_OPT += -DGALLIUM_IRIS \
          -DHAVE_UINT128

include $(REP_DIR)/lib/mk/mesa.inc
