#
# \brief  Build config for Genodes core process
# \author Martin Stein
# \date   2011-12-16
#

# set entry point of core's first thread
CC_OPT += -DCORE_MAIN=_main

# add library dependencies
LIBS += base-common perf_counter

# add include paths
INC_DIR +=  $(REP_DIR)/src/core
INC_DIR +=  $(REP_DIR)/src/core/include
INC_DIR +=  $(REP_DIR)/include
INC_DIR +=  $(REP_DIR)/src/platform
INC_DIR +=  $(REP_DIR)/src/base
INC_DIR += $(BASE_DIR)/src/core/include
INC_DIR += $(BASE_DIR)/include
INC_DIR += $(BASE_DIR)/src/platform
INC_DIR += $(BASE_DIR)/src/base/thread
INC_DIR += $(BASE_DIR)/src/base/include

# add C++ sources
SRC_CC += console.cc
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
SRC_CC += thread.cc
SRC_CC += thread_support.cc
SRC_CC += rm_session_support.cc
SRC_CC += pager.cc
SRC_CC += _main.cc
SRC_CC += cpu_support.cc
SRC_CC += kernel/kernel.cc
SRC_CC += kernel/thread.cc
SRC_CC += kernel/vm.cc
SRC_CC += kernel/signal_receiver.cc
SRC_CC += kernel/irq.cc
SRC_CC += kernel/processor.cc
SRC_CC += kernel/processor_pool.cc

# add assembly sources
SRC_S += mode_transition.s
SRC_S += boot_modules.s
SRC_S += crt0.s

# provide Genode version information
include $(BASE_DIR)/src/core/version.inc

# switch to specific boot-modules if further images shall be available
ifneq ($(wildcard $(BUILD_BASE_DIR)/boot_modules.s),)
  BOOT_MODULES_VPATH = $(BUILD_BASE_DIR)
  INC_DIR += $(BOOT_MODULES_VPATH)
endif
vpath boot_modules.s $(BOOT_MODULES_VPATH)

# declare source locations
vpath _main.cc                    $(BASE_DIR)/src/platform
vpath cpu_session_component.cc    $(BASE_DIR)/src/core
vpath dataspace_component.cc      $(BASE_DIR)/src/core
vpath io_mem_session_component.cc $(BASE_DIR)/src/core
vpath io_mem_session_support.cc   $(BASE_DIR)/src/core
vpath main.cc                     $(BASE_DIR)/src/core
vpath pd_session_component.cc     $(BASE_DIR)/src/core
vpath ram_session_component.cc    $(BASE_DIR)/src/core
vpath rm_session_component.cc     $(BASE_DIR)/src/core
vpath rom_session_component.cc    $(BASE_DIR)/src/core
vpath trace_session_component.cc  $(BASE_DIR)/src/core
vpath dump_alloc.cc               $(BASE_DIR)/src/core
vpath context_area.cc             $(BASE_DIR)/src/core
vpath core_mem_alloc.cc           $(BASE_DIR)/src/core
vpath console.cc                   $(REP_DIR)/src/base
vpath pager.cc                     $(REP_DIR)/src/base
vpath _main.cc                    $(BASE_DIR)/src/platform
vpath thread.cc                   $(BASE_DIR)/src/base/thread
vpath %                            $(REP_DIR)/src/core
