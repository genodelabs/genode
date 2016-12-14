SRC_S       += crt0.s
SRC_CC      += _main.cc init_main_thread.cc
REP_INC_DIR += src/include

vpath %.cc $(BASE_DIR)/src/lib/startup
