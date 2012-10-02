TARGET       = core
REQUIRES     = foc
LIBS         = cxx ipc heap core_printf child pager lock raw_signal raw_server

LD_TEXT_ADDR = 0x500000

GEN_CORE_DIR = $(BASE_DIR)/src/core

SRC_CC       = cap_map.cc \
               cap_session_component.cc \
               context_area.cc \
               cpu_session_component.cc \
               cpu_session_extension.cc \
               dataspace_component.cc \
               dump_alloc.cc \
               io_mem_session_component.cc \
               io_mem_session_support.cc \
               irq_session_component.cc \
               main.cc \
               multiboot_info.cc \
               pd_session_component.cc \
               pd_session_extension.cc \
               platform.cc \
               platform_pd.cc \
               platform_services.cc \
               platform_thread.cc \
               ram_session_component.cc \
               ram_session_support.cc \
               rm_session_component.cc \
               rm_session_support.cc \
               rom_session_component.cc \
               signal_session_component.cc \
               signal_source_component.cc \
               spin_lock.cc \
               thread.cc \
               thread_bootstrap.cc \
               thread_start.cc

INC_DIR     += $(REP_DIR)/src/core/include \
               $(GEN_CORE_DIR)/include \
               $(REP_DIR)/src/base/lock \
               $(BASE_DIR)/src/base/lock

vpath context_area.cc             $(GEN_CORE_DIR)
vpath cpu_session_component.cc    $(GEN_CORE_DIR)
vpath dataspace_component.cc      $(GEN_CORE_DIR)
vpath dump_alloc.cc               $(GEN_CORE_DIR)
vpath io_mem_session_component.cc $(GEN_CORE_DIR)
vpath io_mem_session_support.cc   $(GEN_CORE_DIR)
vpath main.cc                     $(GEN_CORE_DIR)
vpath multiboot_info.cc           $(GEN_CORE_DIR)
vpath pd_session_component.cc     $(GEN_CORE_DIR)
vpath ram_session_component.cc    $(GEN_CORE_DIR)
vpath rm_session_component.cc     $(GEN_CORE_DIR)
vpath rom_session_component.cc    $(GEN_CORE_DIR)
vpath signal_session_component.cc $(GEN_CORE_DIR)
vpath cap_map.cc                  $(REP_DIR)/src/base/env
vpath spin_lock.cc                $(REP_DIR)/src/base/env
vpath thread.cc                   $(REP_DIR)/src/base/thread
vpath thread_bootstrap.cc         $(REP_DIR)/src/base/thread
vpath %.cc                        $(REP_DIR)/src/core
