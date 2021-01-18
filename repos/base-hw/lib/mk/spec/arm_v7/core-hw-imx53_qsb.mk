SRC_CC += kernel/vm_thread_off.cc
SRC_CC += platform_services.cc

# include less specific configuration
include $(call select_from_repositories,lib/mk/spec/arm_v7/core-hw-imx53_qsb.inc)
