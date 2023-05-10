CC_MARCH  = -march=armv8-a -mstrict-align

SRC_CC   += lib/base/arm_64/kernel/interface.cc
SRC_CC   += spec/64bit/memory_map.cc
SRC_S    += bootstrap/spec/arm_64/crt0.s

vpath spec/64bit/memory_map.cc $(call select_from_repositories,src/lib/hw)

include $(call select_from_repositories,lib/mk/bootstrap-hw.inc)
