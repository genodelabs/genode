TARGET   = fb_drv
REQUIRES = exynos
SRC_CC  += main.cc driver.cc
LIBS    += base config server
INC_DIR += $(PRG_DIR)
