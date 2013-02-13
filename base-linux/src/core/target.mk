TARGET        = core
REQUIRES      = linux
LIBS          = cxx ipc heap core_printf child lock raw_server syscall raw_signal

GEN_CORE_DIR  = $(BASE_DIR)/src/core

SRC_CC        = main.cc \
                platform.cc \
                platform_thread.cc \
                platform_services.cc \
                ram_session_component.cc \
                ram_session_support.cc \
                rom_session_component.cc \
                cpu_session_component.cc \
                cpu_session_extension.cc \
                cpu_session_support.cc \
                pd_session_component.cc \
                io_mem_session_component.cc \
                signal_session_component.cc \
                signal_source_component.cc \
                thread.cc \
                thread_linux.cc \
                context_area.cc \
                debug.cc \
                rm_session_mmap.cc

INC_DIR      += $(REP_DIR)/src/core/include \
                $(GEN_CORE_DIR)/include \
                $(REP_DIR)/src/platform \
                $(REP_DIR)/src/base/ipc \
                $(REP_DIR)/src/base/env

HOST_INC_DIR += /usr/include

#
# core does not use POSIX threads when built for the 'lx_hybrid_x86' platform,
# so we need to reserve the thread-context area via a segment in the program to
# prevent clashes with vdso and shared libraries.
#
ifeq ($(findstring always_hybrid, $(SPECS)), always_hybrid)
LD_SCRIPT_STATIC = $(LD_SCRIPT_DEFAULT) \
                   $(call select_from_repositories,src/platform/context_area.stdlib.ld)
endif

vpath main.cc                     $(GEN_CORE_DIR)
vpath thread.cc                   $(BASE_DIR)/src/base/thread
vpath ram_session_component.cc    $(GEN_CORE_DIR)
vpath cpu_session_component.cc    $(GEN_CORE_DIR)
vpath platform_services.cc        $(GEN_CORE_DIR)
vpath signal_session_component.cc $(GEN_CORE_DIR)
vpath signal_source_component.cc  $(GEN_CORE_DIR)
vpath debug.cc                    $(REP_DIR)/src/base/env
vpath rm_session_mmap.cc          $(REP_DIR)/src/base/env
vpath %.cc                        $(PRG_DIR)
