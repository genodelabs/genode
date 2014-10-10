#
# \brief  Offer build configurations that are specific to base-hw
# \author Martin Stein
# \date   2014-02-26
#

# configure multiprocessor mode
CC_OPT += -Wa,--defsym -Wa,NR_OF_CPUS=$(NR_OF_CPUS) -DNR_OF_CPUS=$(NR_OF_CPUS)
