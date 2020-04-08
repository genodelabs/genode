INC_DIR += $(REP_DIR)/src/core/spec/rpi3
INC_DIR += $(REP_DIR)/src/core/spec/arm_v8

# add C++ sources
SRC_CC += kernel/cpu_mp.cc
SRC_CC += kernel/vm_thread_off.cc
SRC_CC += platform_services.cc
SRC_CC += spec/64bit/memory_map.cc
SRC_CC += spec/arm/bcm2837_pic.cc
SRC_CC += spec/arm/generic_timer.cc
SRC_CC += spec/arm/kernel/lock.cc
SRC_CC += spec/arm/platform_support.cc
SRC_CC += spec/arm_v8/cpu.cc
SRC_CC += spec/arm_v8/kernel/cpu.cc
SRC_CC += spec/arm_v8/kernel/thread.cc

#add assembly sources
SRC_S += spec/arm_v8/exception_vector.s
SRC_S += spec/arm_v8/crt0.s

vpath spec/64bit/memory_map.cc $(REP_DIR)/src/lib/hw

NR_OF_CPUS = 4

# include less specific configuration
include $(REP_DIR)/lib/mk/core-hw.inc
