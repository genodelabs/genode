#
# \brief  Enable VFPV3-FPU on ARM
# \author Sebastian Sumpf
# \date   2013-06-16
#

#
# Enable floating point support in compiler
#
CC_MARCH    += -mfpu=vfpv3 -mfloat-abi=softfp

#
# Include floating-point unit code
#
REP_INC_DIR += include/spec/arm/vfp
