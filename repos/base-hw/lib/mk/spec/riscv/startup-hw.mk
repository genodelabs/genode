include $(call select_from_repositories,lib/mk/startup.inc)

vpath crt0.s $(call select_from_repositories,src/lib/startup/spec/riscv)
