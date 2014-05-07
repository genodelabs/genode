#
# Specifics for MagicEyes Digital’s Multimedia Signal Processor
#

#
# Configure target CPU for gcc
#
CC_OPT += -march=armv4t

#
# Defines for L4/sys headers
#
CC_OPT += -DCPUTYPE_mmsp2
L4SYS_ARM_CPU = arm_mmsp2
