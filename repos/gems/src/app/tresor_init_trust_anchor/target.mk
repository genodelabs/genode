TARGET := tresor_init_trust_anchor

SRC_CC += component.cc

INC_DIR += $(PRG_DIR)
INC_DIR += $(REP_DIR)/src/lib/vfs/tresor_trust_anchor
INC_DIR += $(REP_DIR)/src/lib/tresor/include

LIBS += base vfs
