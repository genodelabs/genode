SRC_CC   = extension.cc
REQUIRES = foc
INC_DIR += $(REP_DIR)/src/app/cli_monitor \
           $(REP_DIR)/src/app/cli_monitor/spec/foc

vpath extension.cc $(REP_DIR)/src/app/cli_monitor/spec/foc
