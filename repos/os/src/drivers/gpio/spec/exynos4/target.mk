TARGET   = gpio_drv
REQUIRES = exynos4
SRC_CC   = main.cc
LIBS     = base config server
INC_DIR += $(PRG_DIR)

vpath main.cc $(PRG_DIR)
