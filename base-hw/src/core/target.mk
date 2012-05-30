#
# \brief  The core of Genode
# \author Martin Stein
# \date   2011-12-16
#

# set program name
TARGET = core

# use core specific startup library
STARTUP_LIB = startup_core

# add library dependencies
LIBS += cxx raw_ipc heap process pager lock console signal raw_server \
        syscall startup_core core_support

# add include paths
GEN_CORE_DIR = $(BASE_DIR)/src/core
INC_DIR += $(REP_DIR)/src/core/include $(REP_DIR)/include \
           $(REP_DIR)/src/platform $(GEN_CORE_DIR)/include \
           $(BASE_DIR)/src/platform $(BASE_DIR)/src/core/include \
           $(BASE_DIR)/include

# add C++ sources
SRC_CC += main.cc _main.cc ram_session_component.cc \
          ram_session_support.cc rom_session_component.cc \
          pd_session_component.cc io_mem_session_component.cc \
          io_mem_session_support.cc thread.cc platform_pd.cc platform.cc \
          platform_thread.cc dataspace_component.cc rm_session_component.cc \
          io_port_session_component.cc \
          irq_session_component.cc signal_session_component.cc \
          dump_alloc.cc cpu_session_component.cc \
          cpu_session_support.cc console.cc

# declare file locations
vpath _main.cc                    $(BASE_DIR)/src/platform
vpath main.cc                     $(GEN_CORE_DIR)
vpath ram_session_component.cc    $(GEN_CORE_DIR)
vpath rom_session_component.cc    $(GEN_CORE_DIR)
vpath cpu_session_component.cc    $(GEN_CORE_DIR)
vpath pd_session_component.cc     $(GEN_CORE_DIR)
vpath rm_session_component.cc     $(GEN_CORE_DIR)
vpath io_mem_session_component.cc $(GEN_CORE_DIR)
vpath io_mem_session_support.cc   $(GEN_CORE_DIR)
vpath dataspace_component.cc      $(GEN_CORE_DIR)
vpath dump_alloc.cc               $(GEN_CORE_DIR)
vpath console.cc                  $(REP_DIR)/src/base
vpath %                           $(REP_DIR)/src/core

