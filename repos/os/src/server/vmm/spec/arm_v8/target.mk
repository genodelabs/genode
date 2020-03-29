TARGET    = vmm
REQUIRES  = hw arm_v8
LIBS      = base
SRC_CC   += address_space.cc
SRC_CC   += cpu.cc
SRC_CC   += generic_timer.cc
SRC_CC   += gicv2.cc
SRC_CC   += main.cc
SRC_CC   += mmio.cc
SRC_CC   += pl011.cc
SRC_CC   += virtio_device.cc
SRC_CC   += vm.cc
SRC_CC   += tester.cc
INC_DIR  += $(PRG_DIR)

CC_CXX_WARN_STRICT :=
