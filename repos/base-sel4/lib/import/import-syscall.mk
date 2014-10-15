#
# Access to kernel-interface headers that were installed in the build directory
# when building the platform library.
#
INC_DIR += $(BUILD_BASE_DIR)/include

#
# Access to other sel4-specific headers such as 'autoconf.h'.
#
REP_INC_DIR += include/sel4
