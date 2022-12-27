SHARED_LIB := yes
LIBS       += libdrm

CC_OPT += -DHAVE_UINT128

LIBS   += etnaviv
CC_OPT += -DGALLIUM_ETNAVIV

LIBS   += lima
CC_OPT += -DGALLIUM_LIMA

include $(REP_DIR)/lib/mk/mesa.inc

# use etnaviv_drmif.h from mesa DRM backend
INC_DIR += $(MESA_SRC_DIR)/src/etnaviv
