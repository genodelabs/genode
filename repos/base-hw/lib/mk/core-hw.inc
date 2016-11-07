#
# \brief  Build config for Genodes core process
# \author Martin Stein
# \date   2011-12-16
#

LIBS += base-hw-common cxx

# add include paths
INC_DIR += $(BASE_DIR)/../base-hw/src/core/include
INC_DIR += $(BASE_DIR)/src/core/include
INC_DIR += $(BASE_DIR)/../base-hw/src/include
INC_DIR += $(BASE_DIR)/src/include

# add C++ sources
SRC_CC += cpu_session_component.cc
SRC_CC += cpu_session_support.cc
SRC_CC += cpu_thread_component.cc
SRC_CC += core_log.cc
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
SRC_CC += pd_session_component.cc
SRC_CC += pd_upgrade_ram_quota.cc
SRC_CC += pd_assign_pci.cc
SRC_CC += platform.cc
SRC_CC += platform_pd.cc
SRC_CC += platform_thread.cc
SRC_CC += stack_area.cc
SRC_CC += ram_session_component.cc
SRC_CC += ram_session_support.cc
SRC_CC += region_map_component.cc
SRC_CC += rom_session_component.cc
SRC_CC += trace_session_component.cc
SRC_CC += thread_start.cc
SRC_CC += env.cc
SRC_CC += region_map_support.cc
SRC_CC += pager.cc
SRC_CC += _main.cc
SRC_CC += kernel/cpu_scheduler.cc
SRC_CC += kernel/double_list.cc
SRC_CC += kernel/init.cc
SRC_CC += kernel/thread.cc
SRC_CC += kernel/signal_receiver.cc
SRC_CC += kernel/ipc_node.cc
SRC_CC += kernel/irq.cc
SRC_CC += kernel/pd.cc
SRC_CC += kernel/cpu.cc
SRC_CC += kernel/clock.cc
SRC_CC += kernel/object.cc
SRC_CC += init_main_thread.cc
SRC_CC += capability.cc

# provide Genode version information
include $(BASE_DIR)/src/core/version.inc

# configure multiprocessor mode
NR_OF_CPUS ?= 1
CC_OPT += -Wa,--defsym -Wa,NR_OF_CPUS=$(NR_OF_CPUS) -DNR_OF_CPUS=$(NR_OF_CPUS)

# declare source locations
vpath % $(BASE_DIR)/../base-hw/src/core
vpath % $(BASE_DIR)/src/core
vpath % $(BASE_DIR)/src/lib/startup
