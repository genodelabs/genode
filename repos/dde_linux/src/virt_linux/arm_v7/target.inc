#
# Linux architecture specific kernel configuration
#

# do not generate thumb instructions on ARMv7 platforms
LX_DISABLE += THUMB2_KERNEL ARM_VIRT_EXT DEBUG_PREEMPT

# disable SMP because it will generate unsupported instruction on single core
# Cortex-A8 CPUs
LX_DISABLE += SMP

