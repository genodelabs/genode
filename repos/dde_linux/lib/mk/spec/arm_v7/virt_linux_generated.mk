LINUX_ARCH=arm

# arm_v7 specific kernel configuration
include $(REP_DIR)/src/virt_linux/arm_v7/target.inc

include $(REP_DIR)/lib/mk/virt_linux_generated.inc
