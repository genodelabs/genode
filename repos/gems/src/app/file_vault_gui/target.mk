TARGET  = file_vault_gui
SRC_CC  = main.cc
LIBS   += base dialog sandbox

INC_DIR += $(call select_from_repositories,/src/app/file_vault/include)
INC_DIR += $(call select_from_repositories,/src/lib/tresor/include)
