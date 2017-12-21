TARGET   = test-omap4_gpio_drv
REQUIRES = omap4
SRC_CC   = main.cc
INC_DIR += $(PRG_DIR)
LIBS     = base

vpath main.cc $(PRG_DIR)

CC_CXX_WARN_STRICT :=
