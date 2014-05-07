SRC_CC   = extension.cc
REQUIRES = foc_arndale
INC_DIR += $(REP_DIR)/src/app/cli_monitor \
           $(REP_DIR)/src/app/cli_monitor/foc

vpath extension.cc $(REP_DIR)/src/app/cli_monitor/foc/arndale
