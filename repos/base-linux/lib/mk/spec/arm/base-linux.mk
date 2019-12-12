#
# \brief  ARM-specific base lib parts that are not used by hybrid applications
# \author Christian Prochaska
# \date   2014-05-14
#

SRC_CC += cpu/arm/cache.cc

LIBS += timeout-arm

include $(REP_DIR)/lib/mk/base-linux.mk
