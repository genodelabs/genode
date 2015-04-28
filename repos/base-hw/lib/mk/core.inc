#
# \brief  Build config for Genodes core process
# \author Martin Stein
# \date   2011-12-16
#

# add library dependencies
LIBS += core-perf_counter

# add library dependencies
LIBS += base-common

# add include paths
INC_DIR +=  $(REP_DIR)/src/core/include
INC_DIR += $(BASE_DIR)/src/core/include
INC_DIR += $(BASE_DIR)/src/base/include
INC_DIR += $(BASE_DIR)/src/base/thread
INC_DIR += $(BASE_DIR)/src/platform

# add C++ sources
SRC_CC += console.cc
SRC_CC += cap_session_component.cc
SRC_CC += cpu_session_component.cc
SRC_CC += cpu_session_support.cc
SRC_CC += core_rm_session.cc
SRC_CC += core_mem_alloc.cc
SRC_CC += dataspace_component.cc
SRC_CC += dump_alloc.cc
SRC_CC += io_mem_session_component.cc
SRC_CC += io_mem_session_support.cc
SRC_CC += irq_session_component.cc
SRC_CC += main.cc
SRC_CC += pd_session_component.cc
SRC_CC += platform.cc
SRC_CC += platform_pd.cc
SRC_CC += platform_thread.cc
SRC_CC += context_area.cc
SRC_CC += ram_session_component.cc
SRC_CC += ram_session_support.cc
SRC_CC += rm_session_component.cc
SRC_CC += rom_session_component.cc
SRC_CC += signal_session_component.cc
SRC_CC += trace_session_component.cc
SRC_CC += thread_start.cc
SRC_CC += rm_session_support.cc
SRC_CC += pager.cc
SRC_CC += _main.cc
SRC_CC += kernel/kernel.cc
SRC_CC += kernel/thread.cc
SRC_CC += kernel/signal_receiver.cc
SRC_CC += kernel/irq.cc
SRC_CC += kernel/pd.cc
SRC_CC += kernel/cpu.cc
SRC_CC += init_main_thread.cc

# add assembly sources
SRC_S += boot_modules.s

# provide Genode version information
include $(BASE_DIR)/src/core/version.inc

# switch to build-specific boot-modules if further images shall be available
ifneq ($(wildcard $(BUILD_BASE_DIR)/boot_modules.s),)
  BOOT_MODULES_VPATH = $(BUILD_BASE_DIR)
  INC_DIR += $(BOOT_MODULES_VPATH)
else
  # use dummy boot-modules per default
  BOOT_MODULES_VPATH = $(REP_DIR)/src/core/
endif
vpath boot_modules.s $(BOOT_MODULES_VPATH)

# declare source locations
vpath %  $(REP_DIR)/src/core
vpath % $(BASE_DIR)/src/core
vpath % $(BASE_DIR)/src/platform
