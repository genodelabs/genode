TARGET   = fb_drv
REQUIRES = exynos4
SRC_CC  += main.cc
SRC_CC  += driver.cc 
LIBS    += base config server

# re-using the exynos5 driver for odroid-x2.
INC_DIR += $(REP_DIR)/src/drivers/framebuffer/exynos5/
vpath %.cc $(REP_DIR)/src/drivers/framebuffer/exynos5/

