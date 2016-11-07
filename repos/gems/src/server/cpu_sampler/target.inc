TARGET  = cpu_sampler

SRC_CC += main.cc \
          cpu_session_component.cc \
          cpu_thread_component.cc

INC_DIR = $(REP_DIR)/src/server/cpu_sampler

LIBS    = base

vpath %.cc $(REP_DIR)/src/server/cpu_sampler
