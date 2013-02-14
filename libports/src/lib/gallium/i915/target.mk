TARGET   = gallium-i915
REQUIRES = i915
LIBS     = base gallium-aux gallium-i915
SRC_CC   = main.cc

vpath main.cc $(PRG_DIR)/..
