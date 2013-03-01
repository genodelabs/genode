#
# \brief  TWL6030 Voltage Regulator driver
# \author Alexander Tarasikov <tarasikov@ksyslabs.org>
# \date   2013-02-18
#

TARGET   = twl6030_regulator_drv

# Currently I've only seen it used in TI OMAP SoC
REQUIRES = omap4
SRC_CC   = main.cc
LIBS     = cxx env server
INC_DIR += $(PRG_DIR)

vpath main.cc $(PRG_DIR)

