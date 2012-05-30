#
# \brief  Offer build configurations that are specific to base-hw
# \author Martin Stein
# \date   2012-04-16
#

#
# Denote library that brings the setup sequence for C++ enviroment.
# Also add an according dependency.
#
STARTUP_LIB ?= startup
PRG_LIBS += $(STARTUP_LIB)
