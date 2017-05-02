#
# \brief  Build config for Genodes core process
# \author Stefan Kalkowski
# \author Martin Stein
# \date   2012-10-24
#

# add include paths
INC_DIR += $(REP_DIR)/src/core/spec/imx53_qsb
INC_DIR += $(REP_DIR)/src/core/spec/imx53

SRC_CC += spec/imx53/pic.cc
SRC_CC += spec/imx53/timer.cc
SRC_CC += spec/arm/cpu_context_trustzone.cc

ifneq ($(filter-out $(SPECS),trustzone),)
SRC_CC += kernel/vm_thread_off.cc
SRC_CC += platform_services.cc
else
INC_DIR += $(REP_DIR)/src/core/spec/arm_v7/trustzone
INC_DIR += $(REP_DIR)/src/core/spec/imx53/trustzone

SRC_CC += spec/imx53/trustzone/platform_services.cc
SRC_CC += kernel/vm_thread_on.cc
SRC_CC += spec/arm_v7/trustzone/kernel/vm.cc
SRC_CC += spec/arm_v7/vm_session_component.cc
SRC_CC += spec/arm_v7/trustzone/vm_session_component.cc

SRC_S += spec/arm_v7/trustzone/mode_transition.s
endif

# include less specific configuration
include $(REP_DIR)/lib/mk/spec/cortex_a8/core-hw.inc
