LIBS += syscall

include $(BASE_DIR)/lib/mk/startup.inc

vpath crt0.s $(REP_DIR)/src/platform/x86_32
