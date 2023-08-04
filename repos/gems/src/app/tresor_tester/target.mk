TARGET := tresor_tester

SRC_CC += main.cc

INC_DIR += $(PRG_DIR)
INC_DIR += $(REP_DIR)/src/app/tresor_init/include

LIBS += base
LIBS += tresor
