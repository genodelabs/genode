TARGET        = core
REQUIRES      = linux
LIBS          = cxx base-common syscall startup

GEN_CORE_DIR  = $(BASE_DIR)/src/core

SRC_CC        = main.cc \
                platform.cc \
                platform_thread.cc \
                platform_services.cc \
                ram_session_component.cc \
                ram_session_support.cc \
                rom_session_component.cc \
                cap_session_component.cc \
                cpu_session_component.cc \
                cpu_session_extension.cc \
                cpu_session_support.cc \
                pd_session_component.cc \
                io_mem_session_component.cc \
                signal_session_component.cc \
                signal_source_component.cc \
                trace_session_component.cc \
                thread_linux.cc \
                context_area.cc \
                core_printf.cc \
                thread.cc myself.cc

INC_DIR      += $(REP_DIR)/src/core/include \
                $(GEN_CORE_DIR)/include \
                $(REP_DIR)/src/platform \
                $(REP_DIR)/src/base/ipc \
                $(REP_DIR)/src/base/env \
                $(BASE_DIR)/src/base/env \
                $(REP_DIR)/src/base/console \
                $(BASE_DIR)/src/base/thread \

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

include $(GEN_CORE_DIR)/version.inc

vpath main.cc                     $(GEN_CORE_DIR)
vpath ram_session_component.cc    $(GEN_CORE_DIR)
vpath cap_session_component.cc    $(GEN_CORE_DIR)
vpath cpu_session_component.cc    $(GEN_CORE_DIR)
vpath platform_services.cc        $(GEN_CORE_DIR)
vpath signal_session_component.cc $(GEN_CORE_DIR)
vpath signal_source_component.cc  $(GEN_CORE_DIR)
vpath trace_session_component.cc  $(GEN_CORE_DIR)
vpath core_printf.cc              $(BASE_DIR)/src/base/console
vpath thread.cc                   $(BASE_DIR)/src/base/thread
vpath myself.cc                   $(BASE_DIR)/src/base/thread
vpath trace.cc                    $(BASE_DIR)/src/base/thread
vpath %.cc                        $(PRG_DIR)
