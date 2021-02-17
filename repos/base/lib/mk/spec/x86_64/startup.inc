include $(BASE_DIR)/lib/mk/startup.inc

vpath crt0.s $(call select_from_repositories,src/lib/startup/spec/x86_64)
