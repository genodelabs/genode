TARGET   = themed_decorator
SRC_CC   = main.cc theme.cc window.cc
LIBS     = base libc libpng zlib blit file
INC_DIR += $(PRG_DIR)

CUSTOM_TARGET_DEPS += plain_decorator_theme.tar

BUILD_ARTIFACTS := $(TARGET) plain_decorator_theme.tar

.PHONY: plain_decorator_theme.tar

plain_decorator_theme.tar:
	$(MSG_CONVERT)$@
	$(VERBOSE)tar -cf $@ $(TAR_OPT) -C $(PRG_DIR) theme
	$(VERBOSE)ln -sf $(BUILD_BASE_DIR)/$(PRG_REL_DIR)/$@ $(INSTALL_DIR)/$@
