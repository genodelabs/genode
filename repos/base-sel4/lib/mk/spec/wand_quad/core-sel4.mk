SRC_CC += $(addprefix spec/arm/, boot_info.cc thread.cc platform.cc irq.cc \
                                 vm_space.cc platform_thread.cc)

INC_DIR += $(REP_DIR)/src/core/spec/arm

include $(REP_DIR)/lib/mk/core-sel4.inc

vpath platform_services.cc $(GEN_CORE_DIR)
