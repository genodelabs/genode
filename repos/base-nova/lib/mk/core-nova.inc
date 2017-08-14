LIBS         = base-nova-common cxx

GEN_CORE_DIR = $(BASE_DIR)/src/core

SRC_CC      += stack_area.cc \
               core_mem_alloc.cc \
               core_log.cc \
               core_region_map.cc \
               core_rpc_cap_alloc.cc \
               cpu_session_component.cc \
               cpu_session_support.cc \
               cpu_thread_component.cc \
               dataspace_component.cc \
               default_log.cc \
               dump_alloc.cc \
               echo.cc \
               io_mem_session_component.cc \
               io_mem_session_support.cc \
               io_port_session_component.cc \
               io_port_session_support.cc \
               ipc_pager.cc \
               irq_session_component.cc \
               main.cc \
               pager.cc \
               native_cpu_component.cc \
               native_pd_component.cc \
               pd_session_support.cc \
               rpc_cap_factory.cc \
               ram_dataspace_factory.cc \
               platform.cc \
               platform_rom_modules.cc \
               platform_pd.cc \
               platform_services.cc \
               platform_thread.cc \
               pd_session_component.cc \
               ram_dataspace_support.cc \
               region_map_component.cc \
               rom_session_component.cc \
               thread_start.cc \
               bios_data_area.cc \
               trace_session_component.cc \
               signal_transmitter_noinit.cc \
               signal_receiver.cc

INC_DIR      = $(REP_DIR)/src/core/include \
               $(REP_DIR)/src/include \
               $(BASE_DIR)/src/include \
               $(GEN_CORE_DIR)/include

include $(GEN_CORE_DIR)/version.inc

vpath main.cc                      $(GEN_CORE_DIR)
vpath pd_session_component.cc      $(GEN_CORE_DIR)
vpath rom_session_component.cc     $(GEN_CORE_DIR)
vpath cpu_session_component.cc     $(GEN_CORE_DIR)
vpath cpu_session_support.cc       $(GEN_CORE_DIR)
vpath cpu_thread_component.cc      $(GEN_CORE_DIR)
vpath pd_upgrade_ram_quota.cc      $(GEN_CORE_DIR)
vpath region_map_component.cc      $(GEN_CORE_DIR)
vpath trace_session_component.cc   $(GEN_CORE_DIR)
vpath signal_transmitter_noinit.cc $(GEN_CORE_DIR)
vpath signal_receiver.cc           $(GEN_CORE_DIR)
vpath io_port_session_component.cc $(GEN_CORE_DIR)/spec/x86
vpath io_port_session_support.cc   $(GEN_CORE_DIR)/spec/x86
vpath io_mem_session_component.cc  $(GEN_CORE_DIR)
vpath io_mem_session_support.cc    $(GEN_CORE_DIR)
vpath ram_dataspace_factory.cc     $(GEN_CORE_DIR)
vpath dataspace_component.cc       $(GEN_CORE_DIR)
vpath core_mem_alloc.cc            $(GEN_CORE_DIR)
vpath default_log.cc               $(GEN_CORE_DIR)
vpath dump_alloc.cc                $(GEN_CORE_DIR)
vpath platform_rom_modules.cc      $(GEN_CORE_DIR)
vpath platform_services.cc         $(GEN_CORE_DIR)/spec/x86
vpath stack_area.cc                $(GEN_CORE_DIR)
vpath core_printf.cc               $(BASE_DIR)/src/lib/base
vpath %.cc                         $(REP_DIR)/src/core
