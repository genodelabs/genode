#
# Linux architecture specific kernel configuration
#

LX_ENABLE  += ARCH_MULTI_V6

# do not generate thumb instructions
LX_DISABLE += THUMB2_KERNEL DEBUG_PREEMPT
