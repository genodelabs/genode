#
# Create mirror for architecture-specific L4sys header files
#
L4_INC_TARGETS = arm/l4/sys \
                 arm/l4f/l4/sys \
                 arm/l4/vcpu

include $(REP_DIR)/lib/mk/platform.inc
