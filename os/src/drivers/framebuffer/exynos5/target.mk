TARGET   = fb_drv
REQUIRES = exynos5
SRC_CC  += main.cc driver.cc
SRC_CC  += quirks.cc
LIBS    += base config
INC_DIR += $(PRG_DIR)

ifneq ($(filter foc_arm, $(SPECS)),)
ifneq ($(filter usb, $(SPECS)),)
SPECS += exynos5_fb_foc_usb_quirk
endif
endif
ifneq ($(filter exynos5_fb_foc_usb_quirk, $(SPECS)),)
vpath quirks.cc $(PRG_DIR)/foc_usb
else
vpath quirks.cc $(PRG_DIR)
endif
