TARGET       = core

GEN_CORE_DIR = $(BASE_DIR)/src/core

SRC_CC      += \
               main.cc \
               ram_session_component.cc \
               ram_session_support.cc \
               rom_session_component.cc \
               cpu_session_component.cc \
               cpu_session_support.cc \
               cpu_thread_component.cc \
               pd_session_component.cc \
               rpc_cap_factory.cc \
               pd_assign_pci.cc \
               pd_upgrade_ram_quota.cc \
               io_mem_session_component.cc \
               io_mem_session_support.cc \
               io_port_session_component.cc \
               io_port_session_support.cc \
               thread_start.cc \
               platform_thread.cc \
               platform_pd.cc \
               platform_services.cc \
               platform.cc \
               dataspace_component.cc \
               region_map_component.cc \
               region_map_support.cc \
               irq_session_component.cc \
               signal_source_component.cc \
               trace_session_component.cc \
               core_region_map.cc \
               core_mem_alloc.cc \
               core_rpc_cap_alloc.cc \
               dump_alloc.cc \
               stack_area.cc \
               capability_space.cc \
               pager.cc

LIBS        += core_printf base-common syscall

INC_DIR     += $(REP_DIR)/src/core/include $(GEN_CORE_DIR)/include \
               $(REP_DIR)/src/include      $(BASE_DIR)/src/include

include $(GEN_CORE_DIR)/version.inc

vpath main.cc                     $(GEN_CORE_DIR)
vpath ram_session_component.cc    $(GEN_CORE_DIR)
vpath rom_session_component.cc    $(GEN_CORE_DIR)
vpath cpu_session_component.cc    $(GEN_CORE_DIR)
vpath cpu_session_support.cc      $(GEN_CORE_DIR)
vpath cpu_thread_component.cc     $(GEN_CORE_DIR)
vpath pd_session_component.cc     $(GEN_CORE_DIR)
vpath pd_assign_pci.cc            $(GEN_CORE_DIR)
vpath pd_upgrade_ram_quota.cc     $(GEN_CORE_DIR)
vpath region_map_component.cc     $(GEN_CORE_DIR)
vpath io_mem_session_component.cc $(GEN_CORE_DIR)
vpath io_mem_session_support.cc   $(GEN_CORE_DIR)
vpath io_port_session_component.cc $(GEN_CORE_DIR)/spec/x86
vpath platform_services.cc        $(GEN_CORE_DIR)/spec/x86
vpath trace_session_component.cc  $(GEN_CORE_DIR)
vpath dataspace_component.cc      $(GEN_CORE_DIR)
vpath core_mem_alloc.cc           $(GEN_CORE_DIR)
vpath core_rpc_cap_alloc.cc       $(GEN_CORE_DIR)
vpath dump_alloc.cc               $(GEN_CORE_DIR)
vpath %.cc                        $(REP_DIR)/src/core
