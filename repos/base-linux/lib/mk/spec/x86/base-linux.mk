#
# \brief  x86-specific base lib parts that are not used by hybrid applications
# \author Christian Prochaska
# \date   2014-05-14
#

SRC_CC += cache.cc

LIBS += timeout

include $(REP_DIR)/lib/mk/base-linux.mk
