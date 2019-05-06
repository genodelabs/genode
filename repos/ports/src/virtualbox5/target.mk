TARGET   = virtualbox5

include $(REP_DIR)/src/virtualbox5/target.inc

LIBS    += virtualbox5

vpath frontend/% $(REP_DIR)/src/virtualbox5/
vpath %.cc       $(REP_DIR)/src/virtualbox5/

CC_CXX_WARN_STRICT =
