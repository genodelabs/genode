#
# \brief  Offer build configurations that are specific to base-hw
# \author Martin Stein
# \date   2014-02-26
#

# configure multiprocessor mode
CC_OPT += -Wa,--defsym -Wa,PROCESSORS=$(PROCESSORS) -DPROCESSORS=$(PROCESSORS)
