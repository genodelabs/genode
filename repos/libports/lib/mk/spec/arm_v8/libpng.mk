include $(REP_DIR)/lib/mk/libpng.inc

SRC_C += arm/arm_init.c
SRC_C += arm/filter_neon_intrinsics.c
SRC_C += arm/palette_neon_intrinsics.c

SRC_S += arm/filter_neon.S

vpath %.S $(LIBPNG_DIR)
