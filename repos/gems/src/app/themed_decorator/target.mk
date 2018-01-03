TARGET   = themed_decorator
SRC_CC   = main.cc theme.cc window.cc
LIBS     = base libc libpng zlib blit file
INC_DIR += $(PRG_DIR)

.PHONY: plain_decorator_theme.tar

$(TARGET): plain_decorator_theme.tar
plain_decorator_theme.tar:
	$(VERBOSE)cd $(PRG_DIR); tar cf $(PWD)/bin/$@ theme

CC_CXX_WARN_STRICT =
