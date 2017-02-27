REQUIRES = nova

INC_DIR = $(REP_DIR)/src/server/cpu_sampler

SRC_CC = native_cpu.cc

SHARED_LIB = yes

vpath %.cc $(REP_DIR)/src/lib/cpu_sampler_platform-nova
