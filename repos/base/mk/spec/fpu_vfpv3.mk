#
# Enable floating point support in compiler
#
CC_MARCH += -mfpu=vfpv3 -mfloat-abi=softfp

#
# Include floating-point unit code
#
REP_INC_DIR += include/spec/arm/vfp
