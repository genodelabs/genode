LIBS += syscall-foc

include $(BASE_DIR)/lib/mk/startup.inc

vpath crt0.s $(BASE_DIR)/src/lib/startup/spec/arm_64
