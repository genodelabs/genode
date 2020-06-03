ARCH = $(filter 32bit 64bit,$(SPECS))

INC_DIR += $(call select_from_repositories,src/lib/curl)/spec/$(ARCH)
INC_DIR += $(call select_from_ports,curl)/include
