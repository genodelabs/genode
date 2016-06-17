#
# Access to kernel-interface headers that were installed in the build directory
# when building the platform library.
#
INC_DIR += $(BUILD_BASE_DIR)/include

#
# Access to other sel4-specific headers such as 'autoconf.h'.
#
INC_DIR += $(BUILD_BASE_DIR)/include/sel4

# required for seL4_DebugPutChar
CC_OPT += -DSEL4_DEBUG_KERNEL -DDEBUG
