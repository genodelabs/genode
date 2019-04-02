GEN_SRC_CC = \
             spec/x86/io_port_session_component.cc \
             vm_session_common.cc

REP_SRC_CC = \
             spec/x86/io_port_session_support.cc \
             spec/x86/irq.cc \
             spec/x86/platform_services.cc \
             spec/x86/platform_thread.cc \
             spec/x86/vm_space.cc \
             spec/x86/vm_session_component.cc \
             spec/x86_64/boot_info.cc \
             spec/x86_64/platform.cc \
             spec/x86_64/platform_pd.cc \
             spec/x86_64/thread.cc \
             spec/x86_64/vm_space.cc

INC_DIR += $(REP_DIR)/src/core/spec/x86_64
INC_DIR += $(REP_DIR)/src/core/spec/x86

include $(REP_DIR)/lib/mk/core-sel4.inc
