SRC_CC   = main.cc
LIBS     = base

GENERIC_DRV_DIR = $(dir $(call select_from_repositories,src/driver/uart/target.inc))

INC_DIR += $(PRG_DIR) $(GENERIC_DRV_DIR)

vpath main.cc $(GENERIC_DRV_DIR)
