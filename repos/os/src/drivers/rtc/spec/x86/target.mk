TARGET    = rtc_drv
REQUIRES  = x86
SRC_CC    = main.cc

# enforce hybrid prg on Linux
ifeq ($(filter-out $(SPECS),linux),)
LIBS    = lx_hybrid
SRC_CC += linux.cc
else
LIBS    = base
SRC_CC += rtc.cc
endif
