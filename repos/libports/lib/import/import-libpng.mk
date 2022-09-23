LIBPNG_DIR := $(call select_from_ports,libpng)
INC_DIR += $(call select_from_repositories,include/libpng)
INC_DIR += $(LIBPNG_DIR)/include/libpng
