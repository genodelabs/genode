TARGET   = fb_drv
REQUIRES = exynos5
SRC_CC  += main.cc driver.cc
LIBS    += base
INC_DIR += $(PRG_DIR)

CC_CXX_WARN_STRICT :=
