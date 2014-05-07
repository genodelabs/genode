TARGET   = atapi_drv
REQUIRES = x86
SRC_CC   = main.cc ata_device.cc atapi_device.cc io.cc ata_bus_master.cc
SRC_C    = mindrvr.c
LIBS     = base config server

INC_DIR += $(PRG_DIR)/contrib $(PRG_DIR)

vpath %.c $(PRG_DIR)/contrib
