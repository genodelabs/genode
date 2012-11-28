GEN_CORE_DIR = $(BASE_DIR)/src/core
SPEC_CORE_DIR = $(REP_DIR)/src/core
SPEC_BASE_DIR = $(REP_DIR)/src/base


SRC_CC       = \
               context_area.cc \
               core_rm_session.cc \
               cpu_session_component.cc \
               dataspace_component.cc \
               dump_alloc.cc \
               io_mem_session_component.cc \
               io_mem_session_support.cc \
               irq_session_component.cc \
               main.cc \
               pd_session_component.cc \
               platform.cc \
               platform_services.cc \
               platform_thread.cc \
               ram_session_component.cc \
               ram_session_support.cc \
               rm_session_component.cc \
               rm_session_support.cc \
               rom_session_component.cc \
               signal_session_component.cc \
               signal_source_component.cc \
               thread.cc \
               thread_bootstrap.cc \
               thread_roottask.cc \

INC_DIR      = $(SPEC_CORE_DIR)/include \
               $(GEN_CORE_DIR)/include

vpath context_area.cc             $(SPEC_CORE_DIR)
vpath cpu_session_component.cc    $(GEN_CORE_DIR)
vpath dataspace_component.cc      $(GEN_CORE_DIR)
vpath dump_alloc.cc               $(GEN_CORE_DIR)
vpath io_mem_session_component.cc $(GEN_CORE_DIR)
vpath io_mem_session_support.cc   $(SPEC_CORE_DIR)
vpath irq_session_component.cc    $(SPEC_CORE_DIR)
vpath main.cc                     $(GEN_CORE_DIR)
vpath pd_session_component.cc     $(GEN_CORE_DIR)
vpath platform.cc                 $(GEN_CORE_DIR)
vpath platform_services.cc        $(GEN_CORE_DIR)
vpath platform_thread.cc          $(GEN_CORE_DIR)
vpath ram_session_component.cc    $(GEN_CORE_DIR)
vpath rm_session_component.cc     $(GEN_CORE_DIR)
vpath rom_session_component.cc    $(GEN_CORE_DIR)
vpath signal_session_component.cc $(GEN_CORE_DIR)
vpath signal_source_component.cc  $(GEN_CORE_DIR)
vpath thread.cc                   $(SPEC_BASE_DIR)/thread
vpath thread_bootstrap.cc         $(SPEC_BASE_DIR)/thread
vpath thread_roottask.cc          $(GEN_CORE_DIR)
