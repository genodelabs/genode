include $(call select_from_repositories,lib/mk/startup.inc)

vpath crt0.s $(REP_DIR)/src/lib/startup/spec/riscv
