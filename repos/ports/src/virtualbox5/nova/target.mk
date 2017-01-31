TARGET   = virtualbox5-nova
REQUIRES = nova

LIBS    += virtualbox5-nova

include $(REP_DIR)/src/virtualbox5/target.inc

vpath frontend/% $(REP_DIR)/src/virtualbox5/
vpath %.cc       $(REP_DIR)/src/virtualbox5/
