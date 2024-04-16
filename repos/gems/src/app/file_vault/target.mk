TARGET := file_vault
SRC_CC += main.cc
INC_DIR += $(PRG_DIR) $(PRG_DIR)/include
INC_DIR += $(call select_from_repositories,/src/lib/tresor/include)
LIBS += base sandbox vfs
