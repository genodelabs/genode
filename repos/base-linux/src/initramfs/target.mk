TARGET = lx_init

REQUIRES = x86_64

INITRAMFS = initramfs
INITRAMFS_SRC_C = init.c

EXT_OBJECTS += $(BUILD_BASE_DIR)/initramfs/$(INITRAMFS)

$(TARGET): $(INITRAMFS)

$(INITRAMFS): $(INITRAMFS_SRC_C)
	$(MSG_BUILD)$(INITRAMFS)
	$(VERBOSE)gcc $^ -O0 $(CC_MARCH) -Wall -W -Wextra -Werror -std=gnu99 -o $@ -Wl,-O3 -Wl,--as-needed -static
	$(VERBOSE)ln -sf $(BUILD_BASE_DIR)/initramfs/$(INITRAMFS) $(BUILD_BASE_DIR)/bin/

clean_initramfs:
	$(VERBOSE)rm -rf $(INITRAMFS)

clean: clean_initramfs

vpath init.c $(PRG_DIR)
