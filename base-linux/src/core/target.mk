TARGET        = core
REQUIRES      = linux
LIBS          = cxx ipc heap core_printf process lock raw_server syscall rpath

GEN_CORE_DIR  = $(BASE_DIR)/src/core

SRC_CC        = main.cc \
                platform.cc \
                platform_thread.cc \
                ram_session_component.cc \
                ram_session_support.cc \
                rom_session_component.cc \
                cpu_session_component.cc \
                cpu_session_support.cc \
                pd_session_component.cc \
                io_mem_session_component.cc \
                io_port_session_component.cc \
                signal_session_component.cc \
                signal_source_component.cc \
                thread.cc \
                thread_linux.cc \
                context_area.cc \
                debug.cc

INC_DIR      += $(REP_DIR)/src/core/include \
                $(GEN_CORE_DIR)/include \
                $(REP_DIR)/src/platform

HOST_INC_DIR += /usr/include

vpath main.cc                     $(GEN_CORE_DIR)
vpath thread.cc                   $(BASE_DIR)/src/base/thread
vpath ram_session_component.cc    $(GEN_CORE_DIR)
vpath cpu_session_component.cc    $(GEN_CORE_DIR)
vpath signal_session_component.cc $(GEN_CORE_DIR)
vpath signal_source_component.cc  $(GEN_CORE_DIR)
vpath debug.cc                    $(REP_DIR)/src/base/env
vpath %.cc                        $(PRG_DIR)
