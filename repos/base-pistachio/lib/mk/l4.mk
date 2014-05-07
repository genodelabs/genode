#
# Create symlink to Pistachio's user library
#
#
-include $(BUILD_BASE_DIR)/etc/pistachio.conf

absdir = $(realpath $(shell find $(1) -maxdepth 0 -type d))
PISTACHIO_USER_BUILD_ABS_DIR = $(call absdir,$(PISTACHIO_USER_BUILD_DIR))

$(shell mkdir -p $(LIB_CACHE_DIR)/l4)
$(shell ln -sf $(PISTACHIO_USER_BUILD_ABS_DIR)/lib/libl4.a $(LIB_CACHE_DIR)/l4/l4.lib.a)
