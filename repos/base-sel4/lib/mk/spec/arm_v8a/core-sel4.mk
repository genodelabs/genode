GEN_SRC_CC = platform_services.cc

REP_SRC_CC = \
             spec/arm_v8a/boot_info.cc \
             spec/arm/irq.cc \
             spec/arm_v8a/platform.cc \
             spec/arm/platform_thread.cc \
             spec/arm_v8a/thread.cc \
             spec/arm_v8a/vm_space.cc

INC_DIR += $(REP_DIR)/src/core/spec/arm_v8a

include $(REP_DIR)/lib/mk/core-sel4.inc
