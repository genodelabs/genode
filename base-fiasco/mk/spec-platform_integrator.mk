#
# Specifics for ARM integrator platform
#

#
# Configure target CPU for gcc
#
CC_OPT += -march=armv5

#
# Defines for L4/sys headers
#
CC_OPT += -DCPUTYPE_int
L4SYS_ARM_CPU = arm_int
