TARGET = kernel-fiasco
LIBS   = kernel-fiasco

$(TARGET): sigma0 bootstrap kernel

L4_BUILD_DIR = $(LIB_CACHE_DIR)/syscall-fiasco

kernel:
	$(VERBOSE)ln -sf $(LIB_CACHE_DIR)/kernel-fiasco/build/fiasco $@

sigma0:
	$(VERBOSE)ln -sf $(L4_BUILD_DIR)/bin/x86_586/l4v2/sigma0

bootstrap:
	$(VERBOSE)ln -sf $(L4_BUILD_DIR)/bin/x86_586/bootstrap

clean cleanall:
	$(VERBOSE)rm -f kernel sigma0 bootstrap
