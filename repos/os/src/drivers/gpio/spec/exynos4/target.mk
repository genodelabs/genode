TARGET   = exynos4_gpio_drv
REQUIRES = arm_v7
SRC_CC   = main.cc
LIBS     = base
INC_DIR += $(PRG_DIR)

vpath main.cc $(PRG_DIR)
