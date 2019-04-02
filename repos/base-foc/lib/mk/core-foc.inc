GEN_CORE_DIR = $(BASE_DIR)/src/core

LIBS        += base-foc-common syscall-foc cxx

SRC_CC      += stack_area.cc \
               stack_area_addr.cc \
               core_log.cc \
               core_log_out.cc \
               core_region_map.cc \
               core_rpc_cap_alloc.cc \
               cpu_session_component.cc \
               cpu_session_support.cc \
               cpu_thread_component.cc \
               dataspace_component.cc \
               default_log.cc \
               dump_alloc.cc \
               io_mem_session_component.cc \
               io_mem_session_support.cc \
               ipc_pager.cc \
               irq_session_component.cc \
               main.cc \
               pager.cc \
               pager_object.cc \
               pd_session_support.cc \
               native_cpu_component.cc \
               rpc_cap_factory.cc \
               platform.cc \
               platform_rom_modules.cc \
               platform_pd.cc \
               platform_thread.cc \
               pd_session_component.cc \
               ram_dataspace_support.cc \
               ram_dataspace_factory.cc \
               region_map_component.cc \
               rom_session_component.cc \
               signal_source_component.cc \
               signal_transmitter_proxy.cc \
               signal_receiver.cc \
               thread_start.cc \
               trace_session_component.cc \
               vm_session_component.cc \
               vm_session_common.cc \
               heartbeat.cc

INC_DIR     += $(REP_DIR)/src/core/include \
               $(GEN_CORE_DIR)/include \
               $(REP_DIR)/src/include \
               $(BASE_DIR)/src/include

include $(GEN_CORE_DIR)/version.inc

vpath stack_area.cc               $(GEN_CORE_DIR)
vpath core_log.cc                 $(GEN_CORE_DIR)
vpath cpu_session_component.cc    $(GEN_CORE_DIR)
vpath cpu_session_support.cc      $(GEN_CORE_DIR)
vpath cpu_thread_component.cc     $(GEN_CORE_DIR)
vpath dataspace_component.cc      $(GEN_CORE_DIR)
vpath dump_alloc.cc               $(GEN_CORE_DIR)
vpath default_log.cc              $(GEN_CORE_DIR)
vpath io_mem_session_component.cc $(GEN_CORE_DIR)
vpath io_mem_session_support.cc   $(GEN_CORE_DIR)
vpath main.cc                     $(GEN_CORE_DIR)
vpath pd_session_support.cc       $(GEN_CORE_DIR)
vpath pd_upgrade_ram_quota.cc     $(GEN_CORE_DIR)
vpath pd_session_component.cc     $(GEN_CORE_DIR)
vpath region_map_component.cc     $(GEN_CORE_DIR)
vpath rom_session_component.cc    $(GEN_CORE_DIR)
vpath trace_session_component.cc  $(GEN_CORE_DIR)
vpath ram_dataspace_factory.cc    $(GEN_CORE_DIR)
vpath signal_transmitter_proxy.cc $(GEN_CORE_DIR)
vpath signal_receiver.cc          $(GEN_CORE_DIR)
vpath core_rpc_cap_alloc.cc       $(GEN_CORE_DIR)
vpath core_region_map.cc          $(GEN_CORE_DIR)
vpath platform_rom_modules.cc     $(GEN_CORE_DIR)
vpath heartbeat.cc                $(GEN_CORE_DIR)
vpath vm_session_common.cc        $(GEN_CORE_DIR)
vpath %.cc                        $(REP_DIR)/src/core
vpath %.cc                        $(REP_DIR)/src/lib/base
