TARGET   = virtualbox-muen
REQUIRES = muen

LIBS    += virtualbox-muen

include $(REP_DIR)/src/virtualbox/target.inc

vpath frontend/% $(REP_DIR)/src/virtualbox/
vpath %.cc       $(REP_DIR)/src/virtualbox/
