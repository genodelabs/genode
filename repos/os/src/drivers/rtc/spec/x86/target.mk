TARGET    = rtc_drv
REQUIRES  = x86
SRC_CC    = main.cc
LIBS      = base

# enforce hybrid prg on Linux
ifeq ($(filter-out $(SPECS),linux),)
SRC_CC += linux.cc
LIBS   += lx_hybrid
else
SRC_CC += rtc.cc
endif
