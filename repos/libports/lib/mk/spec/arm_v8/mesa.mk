SHARED_LIB := yes
LIBS       += libdrm

LIBS += etnaviv

CC_OPT += -DGALLIUM_ETNAVIV \
          -DHAVE_UINT128

include $(REP_DIR)/lib/mk/mesa.inc

# use etnaviv_drmif.h from mesa DRM backend
INC_DIR += $(MESA_SRC_DIR)/src/etnaviv
