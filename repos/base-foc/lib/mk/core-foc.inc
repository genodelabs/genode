GEN_CORE_DIR = $(BASE_DIR)/src/core

LIBS        += base-foc-common syscall-foc cxx

SRC_CC      += stack_area.cc \
               core_log.cc \
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
               pd_session_component.cc \
               pd_assign_pci.cc \
               pd_upgrade_ram_quota.cc \
               native_pd_component.cc \
               native_cpu_component.cc \
               rpc_cap_factory.cc \
               platform.cc \
               platform_pd.cc \
               platform_services.cc \
               platform_thread.cc \
               ram_session_component.cc \
               ram_session_support.cc \
               region_map_component.cc \
               region_map_support.cc \
               rom_session_component.cc \
               signal_source_component.cc \
               thread_start.cc \
               trace_session_component.cc

INC_DIR     += $(REP_DIR)/src/core/include \
               $(GEN_CORE_DIR)/include \
               $(REP_DIR)/src/include \
               $(BASE_DIR)/src/include

include $(GEN_CORE_DIR)/version.inc

vpath stack_area.cc               $(GEN_CORE_DIR)
vpath cpu_session_component.cc    $(GEN_CORE_DIR)
vpath cpu_session_support.cc      $(GEN_CORE_DIR)
vpath cpu_thread_component.cc     $(GEN_CORE_DIR)
vpath dataspace_component.cc      $(GEN_CORE_DIR)
vpath dump_alloc.cc               $(GEN_CORE_DIR)
vpath default_log.cc              $(GEN_CORE_DIR)
vpath io_mem_session_component.cc $(GEN_CORE_DIR)
vpath io_mem_session_support.cc   $(GEN_CORE_DIR)
vpath main.cc                     $(GEN_CORE_DIR)
vpath pd_session_component.cc     $(GEN_CORE_DIR)
vpath pd_assign_pci.cc            $(GEN_CORE_DIR)
vpath pd_upgrade_ram_quota.cc     $(GEN_CORE_DIR)
vpath ram_session_component.cc    $(GEN_CORE_DIR)
vpath region_map_component.cc     $(GEN_CORE_DIR)
vpath rom_session_component.cc    $(GEN_CORE_DIR)
vpath trace_session_component.cc  $(GEN_CORE_DIR)
vpath core_rpc_cap_alloc.cc       $(GEN_CORE_DIR)
vpath core_region_map.cc          $(GEN_CORE_DIR)
vpath %.cc                        $(REP_DIR)/src/core
vpath %.cc                        $(REP_DIR)/src/lib/base
