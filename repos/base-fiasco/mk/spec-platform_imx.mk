#
# Specifics for Freescale i.MX21 platform
#

RAM_BASE = 0xc0000000

#
# Configure target CPU for gcc
#
CC_OPT += -march=armv5

#
# Defines for L4/sys headers
#
CC_OPT += -DCPUTYPE_imx
L4SYS_ARM_CPU = arm_imx
