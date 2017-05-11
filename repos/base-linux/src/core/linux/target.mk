TARGET        = core-linux
REQUIRES      = linux
LIBS          = cxx base-linux-common syscall-linux startup-linux

GEN_CORE_DIR  = $(BASE_DIR)/src/core

SRC_CC        = main.cc \
                platform.cc \
                platform_thread.cc \
                platform_services.cc \
                pd_session_component.cc \
                ram_dataspace_support.cc \
                rom_session_component.cc \
                cpu_session_component.cc \
                cpu_session_support.cc \
                cpu_thread_component.cc \
                pd_upgrade_ram_quota.cc \
                pd_assign_pci.cc \
                dataspace_component.cc \
                native_pd_component.cc \
                native_cpu_component.cc \
                capability_space.cc \
                rpc_cap_factory_l4.cc \
                ram_dataspace_factory.cc \
                core_rpc_cap_alloc.cc \
                io_mem_session_component.cc \
                signal_source_component.cc \
                signal_transmitter_proxy.cc \
                signal_receiver.cc \
                trace_session_component.cc \
                thread_linux.cc \
                stack_area.cc \
                core_log.cc \
                default_log.cc \
                env_reinit.cc \
                thread.cc thread_myself.cc

INC_DIR      += $(REP_DIR)/src/core/include \
                $(GEN_CORE_DIR)/include \
                $(REP_DIR)/src/platform \
                $(REP_DIR)/src/include \
                $(BASE_DIR)/src/include

LD_TEXT_ADDR     ?= 0x01000000
LD_SCRIPT_STATIC  = $(BASE_DIR)/src/ld/genode.ld \
                    $(call select_from_repositories,src/ld/stack_area.nostdlib.ld)

include $(GEN_CORE_DIR)/version.inc

vpath main.cc                     $(GEN_CORE_DIR)
vpath pd_session_component.cc     $(GEN_CORE_DIR)
vpath cpu_session_component.cc    $(GEN_CORE_DIR)
vpath cpu_session_support.cc      $(GEN_CORE_DIR)
vpath cpu_thread_component.cc     $(GEN_CORE_DIR)
vpath pd_upgrade_ram_quota.cc     $(GEN_CORE_DIR)
vpath pd_assign_pci.cc            $(GEN_CORE_DIR)
vpath capability_space.cc         $(GEN_CORE_DIR)
vpath rpc_cap_factory_l4.cc       $(GEN_CORE_DIR)
vpath ram_dataspace_factory.cc    $(GEN_CORE_DIR)
vpath platform_services.cc        $(GEN_CORE_DIR)
vpath signal_source_component.cc  $(GEN_CORE_DIR)
vpath signal_transmitter_proxy.cc $(GEN_CORE_DIR)
vpath signal_receiver.cc          $(GEN_CORE_DIR)
vpath trace_session_component.cc  $(GEN_CORE_DIR)
vpath core_rpc_cap_alloc.cc       $(GEN_CORE_DIR)
vpath default_log.cc              $(GEN_CORE_DIR)
vpath thread.cc                   $(BASE_DIR)/src/lib/base
vpath thread_myself.cc            $(BASE_DIR)/src/lib/base
vpath trace.cc                    $(BASE_DIR)/src/lib/base
vpath env_reinit.cc               $(REP_DIR)/src/lib/base
vpath %.cc                        $(REP_DIR)/src/core
