#
# \brief  Framebuffer driver specific for OMAP44xx systems
# \author Martin Stein
# \date   2012-05-02
#

TARGET   = fb_drv
REQUIRES = omap4
SRC_CC   = main.cc
LIBS     = base blit
INC_DIR += $(PRG_DIR)

vpath main.cc $(PRG_DIR)

CC_CXX_WARN_STRICT =
