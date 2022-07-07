SHARED_LIB := yes
LIBS       += libdrm-lima

LIBS += lima

CC_OPT += -DGALLIUM_LIMA \
          -DHAVE_UINT128

include $(REP_DIR)/lib/mk/mesa.inc
