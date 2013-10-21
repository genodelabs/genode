TARGET   = fb_drv
REQUIRES = exynos5
SRC_CC  += main.cc driver.cc
LIBS    += base config
INC_DIR += $(PRG_DIR)
