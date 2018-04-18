GEN_SRC_CC = platform_services.cc

REP_SRC_CC = \
             spec/arm/boot_info.cc \
             spec/arm/irq.cc \
             spec/arm/platform.cc \
             spec/arm/platform_thread.cc \
             spec/arm/thread.cc \
             spec/arm/vm_space.cc

INC_DIR += $(REP_DIR)/src/core/spec/arm

include $(REP_DIR)/lib/mk/core-sel4.inc
