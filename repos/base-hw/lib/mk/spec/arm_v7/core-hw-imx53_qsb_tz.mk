INC_DIR += $(REP_DIR)/src/core/spec/arm_v7/trustzone
INC_DIR += $(REP_DIR)/src/core/spec/imx53/trustzone

SRC_CC += kernel/vm_thread_on.cc
SRC_CC += spec/arm_v7/trustzone/kernel/vm.cc
SRC_CC += spec/arm_v7/trustzone/platform_services.cc
SRC_CC += spec/arm_v7/trustzone/vm_session_component.cc
SRC_CC += vm_session_common.cc
SRC_CC += vm_session_component.cc

SRC_S += spec/arm_v7/trustzone/exception_vector.s

# include less specific configuration
include $(REP_DIR)/lib/mk/spec/arm_v7/core-hw-imx53_qsb.inc

