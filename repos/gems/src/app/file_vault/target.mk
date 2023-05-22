TARGET := file_vault

SRC_CC += main.cc menu_view_dialog.cc capacity.cc

INC_DIR += $(PRG_DIR)
INC_DIR += $(REP_DIR)/src/lib/tresor/include

LIBS += base sandbox vfs

CC_OPT += -Os
