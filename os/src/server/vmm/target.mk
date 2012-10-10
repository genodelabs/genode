TARGET    = vmm
REQUIRES  = trustzone platform_vea9x4
LIBS      = env cxx elf signal
SRC_CC    = main.cc
INC_DIR  += $(PRG_DIR)/include