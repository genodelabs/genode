TARGET   = ahci_bench
REQUIRES = exynos5
SRC_CC   = main.cc ahci_driver.cc
LIBS     = base
INC_DIR += $(PRG_DIR)/.. $(PRG_DIR)/../..
vpath ahci_driver.cc $(PRG_DIR)/..
