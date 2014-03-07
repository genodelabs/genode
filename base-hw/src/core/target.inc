#
# \brief  The core of Genode
# \author Martin Stein
# \date   2011-12-16
#

# set program name
TARGET = core

# set entry point of core's first thread
CC_OPT += -DCORE_MAIN=_main

# add library dependencies
LIBS += base-common perf_counter

# add include paths
INC_DIR += $(REP_DIR)/src/core \
           $(REP_DIR)/src/core/include \
           $(REP_DIR)/include \
           $(REP_DIR)/src/platform \
           $(REP_DIR)/src/base \
           $(BASE_DIR)/src/core/include \
           $(BASE_DIR)/include \
           $(BASE_DIR)/src/platform \
           $(BASE_DIR)/src/base/thread \
           $(BASE_DIR)/src/base/include

# add C++ sources
SRC_CC += console.cc \
          cpu_session_component.cc \
          cpu_session_support.cc \
          dataspace_component.cc \
          dump_alloc.cc \
          io_mem_session_component.cc \
          io_mem_session_support.cc \
          irq_session_component.cc \
          main.cc \
          pd_session_component.cc \
          platform.cc \
          platform_pd.cc \
          platform_thread.cc \
          context_area.cc \
          ram_session_component.cc \
          ram_session_support.cc \
          rm_session_component.cc \
          rom_session_component.cc \
          signal_session_component.cc \
          trace_session_component.cc \
          thread.cc \
          kernel/kernel.cc \
          kernel/thread.cc \
          kernel/vm.cc \
          kernel/signal_receiver.cc \
          kernel/irq.cc \
          kernel/scheduler.cc \
          kernel/processor.cc \
          processor_broadcast.cc \
          rm_session_support.cc \
          trustzone.cc \
          pager.cc \
          _main.cc

# declare file locations
vpath _main.cc                    $(BASE_DIR)/src/platform
vpath cpu_session_component.cc    $(BASE_DIR)/src/core
vpath dataspace_component.cc      $(BASE_DIR)/src/core
vpath io_mem_session_component.cc $(BASE_DIR)/src/core
vpath io_mem_session_support.cc   $(BASE_DIR)/src/core
vpath main.cc                     $(BASE_DIR)/src/core
vpath pd_session_component.cc     $(BASE_DIR)/src/core
vpath ram_session_component.cc    $(BASE_DIR)/src/core
vpath rm_session_component.cc     $(BASE_DIR)/src/core
vpath rom_session_component.cc    $(BASE_DIR)/src/core
vpath trace_session_component.cc  $(BASE_DIR)/src/core
vpath dump_alloc.cc               $(BASE_DIR)/src/core
vpath console.cc                  $(REP_DIR)/src/base
vpath pager.cc                    $(REP_DIR)/src/base
vpath _main.cc                    $(BASE_DIR)/src/platform
vpath %                           $(REP_DIR)/src/core

