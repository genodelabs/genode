TARGET       = core
LIBS         = cxx ipc heap core_printf process pager lock \
               raw_signal raw_server

GEN_CORE_DIR = $(BASE_DIR)/src/core

SRC_CC       = \
               main.cc \
               ram_session_component.cc \
               ram_session_support.cc \
               rom_session_component.cc \
               cpu_session_component.cc \
               cpu_session_support.cc \
               pd_session_component.cc \
               io_mem_session_component.cc \
               io_mem_session_support.cc \
               thread.cc \
               thread_start.cc \
               platform_thread.cc \
               platform_pd.cc \
               platform.cc \
               core_mem_alloc.cc \
               dataspace_component.cc \
               rm_session_component.cc \
               rm_session_support.cc \
               io_port_session_component.cc \
               irq_session_component.cc \
               signal_session_component.cc \
               signal_source_component.cc \
               core_rm_session.cc \
               cap_sel_alloc.cc \
               main_thread.cc \
               context_area.cc \
               echo.cc \
               dump_alloc.cc

INC_DIR      = $(REP_DIR)/src/core/include \
               $(GEN_CORE_DIR)/include

vpath main.cc                      $(GEN_CORE_DIR)
vpath ram_session_component.cc     $(GEN_CORE_DIR)
vpath rom_session_component.cc     $(GEN_CORE_DIR)
vpath cpu_session_component.cc     $(GEN_CORE_DIR)
vpath pd_session_component.cc      $(GEN_CORE_DIR)
vpath rm_session_component.cc      $(GEN_CORE_DIR)
vpath signal_session_component.cc  $(GEN_CORE_DIR)
vpath io_port_session_component.cc $(GEN_CORE_DIR)/x86
vpath io_mem_session_component.cc  $(GEN_CORE_DIR)
vpath io_mem_session_support.cc    $(GEN_CORE_DIR)
vpath dataspace_component.cc       $(GEN_CORE_DIR)
vpath core_mem_alloc.cc            $(GEN_CORE_DIR)
vpath dump_alloc.cc                $(GEN_CORE_DIR)
vpath context_area.cc              $(GEN_CORE_DIR)
vpath %.cc                         $(REP_DIR)/src/core
vpath thread.cc                    $(BASE_DIR)/src/base/thread
vpath cap_sel_alloc.cc             $(REP_DIR)/src/base/env
vpath main_thread.cc               $(REP_DIR)/src/base/env
