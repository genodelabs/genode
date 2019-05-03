TARGET   = exynos5_fb_drv
REQUIRES = arm_v7
SRC_CC  += main.cc driver.cc
LIBS    += base
INC_DIR += $(PRG_DIR)
INC_DIR += $(call select_from_repositories,include/spec/exynos5)

CC_CXX_WARN_STRICT :=
