TARGET    = vmm
REQUIRES  = hw arm_v7
LIBS      = base
SRC_CC   += spec/arm_v7/generic_timer.cc
SRC_CC   += address_space.cc
SRC_CC   += cpu.cc
SRC_CC   += cpu_base.cc
SRC_CC   += generic_timer.cc
SRC_CC   += gic.cc
SRC_CC   += main.cc
SRC_CC   += mmio.cc
SRC_CC   += pl011.cc
SRC_CC   += vm.cc
SRC_CC   += virtio_device.cc
INC_DIR  += $(PRG_DIR)/../.. $(PRG_DIR)

vpath %.cc $(PRG_DIR)/../..

CC_CXX_WARN_STRICT :=
