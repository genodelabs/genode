#
# \brief  Build config for Genodes core process
# \author Martin Stein
# \date   2011-12-16
#

LIBS += base-hw-common cxx

# add include paths
REP_INC_DIR += src/core
REP_INC_DIR += src/core/include
REP_INC_DIR += src/include

# add C++ sources
SRC_CC += cpu_session_component.cc
SRC_CC += cpu_session_support.cc
SRC_CC += cpu_thread_component.cc
SRC_CC += core_log.cc
SRC_CC += core_log_out.cc
SRC_CC += core_region_map.cc
SRC_CC += core_mem_alloc.cc
SRC_CC += core_rpc_cap_alloc.cc
SRC_CC += dataspace_component.cc
SRC_CC += default_log.cc
SRC_CC += dump_alloc.cc
SRC_CC += kernel_log.cc
SRC_CC += io_mem_session_component.cc
SRC_CC += io_mem_session_support.cc
SRC_CC += irq_session_component.cc
SRC_CC += main.cc
SRC_CC += native_pd_component.cc
SRC_CC += native_utcb.cc
SRC_CC += pd_session_support.cc
SRC_CC += platform.cc
SRC_CC += platform_rom_modules.cc
SRC_CC += platform_pd.cc
SRC_CC += platform_thread.cc
SRC_CC += stack_area.cc
SRC_CC += pd_session_component.cc
SRC_CC += ram_dataspace_support.cc
SRC_CC += region_map_component.cc
SRC_CC += rom_session_component.cc
SRC_CC += trace_session_component.cc
SRC_CC += signal_receiver.cc
SRC_CC += ram_dataspace_factory.cc
SRC_CC += signal_transmitter_noinit.cc
SRC_CC += thread_start.cc
SRC_CC += env.cc
SRC_CC += region_map_support.cc
SRC_CC += pager.cc
SRC_CC += _main.cc
SRC_CC += kernel/cpu.cc
SRC_CC += kernel/scheduler.cc
SRC_CC += kernel/ipc_node.cc
SRC_CC += kernel/irq.cc
SRC_CC += kernel/main.cc
SRC_CC += kernel/object.cc
SRC_CC += kernel/signal_receiver.cc
SRC_CC += kernel/thread.cc
SRC_CC += kernel/timer.cc
SRC_CC += capability.cc
SRC_CC += stack_area_addr.cc
SRC_CC += heartbeat.cc

# provide Genode version information
include $(BASE_DIR)/src/core/version.inc

TMP         := $(call select_from_repositories,lib/mk/core-hw.inc)
BASE_HW_DIR := $(TMP:%/lib/mk/core-hw.inc=%)

# declare source locations
vpath % $(BASE_HW_DIR)/src/core
vpath % $(BASE_DIR)/src/core
vpath % $(BASE_DIR)/src/lib/startup

include $(call select_from_repositories,lib/mk/consts-hw.inc)
