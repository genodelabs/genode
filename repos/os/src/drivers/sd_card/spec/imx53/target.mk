TARGET    = sd_card_drv
REQUIRES += imx53
SRC_CC   += main.cc
SRC_CC   += adma2.cc
SRC_CC   += esdhcv2.cc
LIBS     += base
INC_DIR  += $(PRG_DIR)
INC_DIR  += $(PRG_DIR)/../../
