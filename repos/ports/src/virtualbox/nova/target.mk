TARGET   = virtualbox-nova
REQUIRES = nova

LIBS    += virtualbox-nova

include $(REP_DIR)/src/virtualbox/target.inc

vpath frontend/% $(REP_DIR)/src/virtualbox/
vpath %.cc       $(REP_DIR)/src/virtualbox/
