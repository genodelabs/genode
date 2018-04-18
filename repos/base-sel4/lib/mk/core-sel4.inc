GEN_CORE_DIR = $(dir $(call select_from_repositories,src/core/main.cc))
BASE_REP_DIR = $(GEN_CORE_DIR)/../..

GEN_SRC_CC += \
              core_log.cc \
              core_mem_alloc.cc \
              core_rpc_cap_alloc.cc \
              cpu_session_component.cc \
              cpu_session_support.cc \
              cpu_thread_component.cc \
              dataspace_component.cc \
              default_log.cc \
              dump_alloc.cc \
              io_mem_session_component.cc \
              main.cc \
              pd_session_component.cc \
              pd_session_support.cc \
              platform_thread.cc \
              ram_dataspace_factory.cc \
              region_map_component.cc \
              rom_session_component.cc \
              signal_receiver.cc \
              signal_transmitter_proxy.cc \
              trace_session_component.cc

REP_SRC_CC += \
              capability_space.cc \
              core_log_out.cc \
              core_region_map.cc \
              io_mem_session_support.cc \
              irq_session_component.cc \
              pager.cc \
              platform.cc \
              platform_pd.cc \
              platform_thread.cc \
              ram_dataspace_support.cc \
              rpc_cap_factory.cc \
              signal_source_component.cc \
              stack_area.cc \
              thread_start.cc

SRC_CC += $(GEN_SRC_CC) $(REP_SRC_CC)

LIBS        += base-sel4-common syscall-sel4 cxx

INC_DIR     += $(REP_DIR)/src/core/include $(GEN_CORE_DIR)/include \
               $(REP_DIR)/src/include      $(BASE_REP_DIR)/src/include

include $(GEN_CORE_DIR)/version.inc

_vpath = $(foreach f,$(1),$(eval vpath $(f) $(2)))

$(call _vpath,$(GEN_SRC_CC),$(GEN_CORE_DIR))
$(call _vpath,$(REP_SRC_CC),$(REP_DIR)/src/core)
