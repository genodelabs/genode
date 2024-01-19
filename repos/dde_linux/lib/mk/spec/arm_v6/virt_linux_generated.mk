LINUX_ARCH=arm

# arm_v6 specific kernel configuration
include $(REP_DIR)/src/virt_linux/arm_v6/target.inc

include $(REP_DIR)/lib/mk/virt_linux_generated.inc
