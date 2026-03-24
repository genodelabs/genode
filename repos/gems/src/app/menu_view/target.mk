TARGET   = menu_view
SRC_CC   = main.cc
LIBS     = base libc libm vfs libpng zlib blit file
INC_DIR += $(PRG_DIR)

CC_OLEVEL := -O3

CUSTOM_TARGET_DEPS += menu_view_style.tar

BUILD_ARTIFACTS := $(TARGET) menu_view_style.tar

.PHONY: menu_view_style.tar

menu_view_style.tar:
	$(MSG_CONVERT)$@
	$(VERBOSE)tar $(TAR_OPT) -cf $@ -C $(PRG_DIR) style
	$(VERBOSE)ln -sf $(BUILD_BASE_DIR)/$(PRG_REL_DIR)/$@ $(INSTALL_DIR)/$@
