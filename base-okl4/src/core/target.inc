TARGET       = core
REQUIRES     = okl4
LIBS         = cxx ipc heap core_printf child pager lock \
               raw_signal raw_server bootinfo child

GEN_CORE_DIR = $(BASE_DIR)/src/core

SRC_CC       = main.cc \
               ram_session_component.cc \
               ram_session_support.cc \
               rom_session_component.cc \
               cpu_session_component.cc \
               cpu_session_support.cc \
               pd_session_component.cc \
               okl4_pd_session_component.cc \
               io_mem_session_component.cc \
               io_mem_session_support.cc \
               thread.cc \
               thread_start.cc \
               thread_bootstrap.cc \
               platform_thread.cc \
               platform_pd.cc \
               platform.cc \
               dataspace_component.cc \
               rm_session_component.cc \
               rm_session_support.cc \
               io_port_session_component.cc \
               irq_session_component.cc \
               signal_session_component.cc \
               signal_source_component.cc \
               core_rm_session.cc \
               core_mem_alloc.cc \
               dump_alloc.cc \
               context_area.cc

INC_DIR      = $(REP_DIR)/src/core/include \
               $(GEN_CORE_DIR)/include

vpath main.cc                     $(GEN_CORE_DIR)
vpath ram_session_component.cc    $(GEN_CORE_DIR)
vpath rom_session_component.cc    $(GEN_CORE_DIR)
vpath cpu_session_component.cc    $(GEN_CORE_DIR)
vpath pd_session_component.cc     $(GEN_CORE_DIR)
vpath rm_session_component.cc     $(GEN_CORE_DIR)
vpath io_mem_session_component.cc $(GEN_CORE_DIR)
vpath io_mem_session_support.cc   $(GEN_CORE_DIR)
vpath signal_session_component.cc $(GEN_CORE_DIR)
vpath signal_source_component.cc  $(GEN_CORE_DIR)
vpath dataspace_component.cc      $(GEN_CORE_DIR)
vpath core_mem_alloc.cc           $(GEN_CORE_DIR)
vpath dump_alloc.cc               $(GEN_CORE_DIR)
vpath context_area.cc             $(GEN_CORE_DIR)
vpath %.cc                        $(REP_DIR)/src/core
vpath thread_bootstrap.cc         $(REP_DIR)/src/base/thread
vpath thread_start.cc             $(BASE_DIR)/src/base/thread
vpath thread.cc                   $(BASE_DIR)/src/base/thread
