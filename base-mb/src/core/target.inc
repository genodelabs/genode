GEN_CORE_DIR = $(BASE_DIR)/src/core
SPEC_CORE_DIR = $(REP_DIR)/src/core
SPEC_BASE_DIR = $(REP_DIR)/src/base


SRC_CC       = \
               main.cc \
               ram_session_component.cc \
               ram_session_support.cc \
               rom_session_component.cc \
               cpu_session_component.cc \
               pd_session_component.cc \
               io_mem_session_component.cc \
               io_mem_session_support.cc \
               thread.cc \
               thread_roottask.cc \
               thread_bootstrap.cc \
               platform_thread.cc \
               platform.cc \
               dataspace_component.cc \
               rm_session_component.cc \
               rm_session_support.cc \
               io_port_session_component.cc \
               irq_session_component.cc \
               signal_session_component.cc \
               signal_source_component.cc \
               core_rm_session.cc \
               dump_alloc.cc \
               context_area.cc

INC_DIR      = $(SPEC_CORE_DIR)/include \
               $(GEN_CORE_DIR)/include 

vpath main.cc                     $(GEN_CORE_DIR)
vpath ram_session_component.cc    $(GEN_CORE_DIR)
vpath rom_session_component.cc    $(GEN_CORE_DIR)
vpath cpu_session_component.cc    $(GEN_CORE_DIR)
vpath pd_session_component.cc     $(GEN_CORE_DIR)
vpath rm_session_component.cc     $(GEN_CORE_DIR)
vpath io_mem_session_component.cc $(GEN_CORE_DIR)
vpath io_mem_session_support.cc   $(SPEC_CORE_DIR)
vpath signal_session_component.cc $(GEN_CORE_DIR)
vpath signal_source_component.cc  $(GEN_CORE_DIR)
vpath dataspace_component.cc      $(GEN_CORE_DIR)
vpath dump_alloc.cc               $(GEN_CORE_DIR)
vpath context_area.cc             $(SPEC_CORE_DIR)
vpath platform.cc                 $(GEN_CORE_DIR)
vpath platform_thread.cc          $(GEN_CORE_DIR)
vpath thread_roottask.cc          $(GEN_CORE_DIR)
vpath thread_bootstrap.cc         $(SPEC_BASE_DIR)/thread
vpath thread.cc                   $(SPEC_BASE_DIR)/thread
vpath irq_session_component.cc    $(SPEC_CORE_DIR)
