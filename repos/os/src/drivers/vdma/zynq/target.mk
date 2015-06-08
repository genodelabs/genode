# \brief  VDMA specific for zynq systems
# \author Mark Albers
# \date   2015-04-13

TARGET   = zynq_vdma_drv

SRC_CC   = main.cc
LIBS     = base config libc libm stdcxx
INC_DIR += $(PRG_DIR)

vpath main.cc $(PRG_DIR)

