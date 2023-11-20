#
# The following variables must be defined by the caller:
#
#   SYMBOLS          - path to symbols file
#   LIB_PROGRESS_LOG - record of visited ABIs
#   LIB_DEP_FILE     - destination Makefile for ABI-creation rule
#

override LIB := $(notdir $(SYMBOLS))

all:
	@true

include $(BASE_DIR)/mk/dep.inc
include $(LIB_PROGRESS_LOG)

ifeq ($(filter $(ABIS_READY),$(LIB)),)
all: generate
endif

log_progress:
	@echo "ABIS_READY += $(LIB)" >> $(LIB_PROGRESS_LOG)

generate:
	@(echo "SO_NAME($(LIB)) := $(LIB).lib.so"; \
	  echo "$(LIB).lib.a:"; \
	  echo "	@true"; \
	  echo "$(LIB).abi.so:"; \
	  echo "	\$$(VERBOSE)\$$(call _prepare_lib_step,\$$@,$(LIB),)"; \
	  echo "	\$$(VERBOSE_MK)\$$(MAKE) \$$(VERBOSE_DIR) -C \$$(LIB_CACHE_DIR)/$(LIB) -f \$$(BASE_DIR)/mk/abi.mk \\"; \
	  echo "	     SYMBOLS=$(SYMBOLS) \\"; \
	  echo "	     LIB=$(LIB) \\"; \
	  echo "	     BUILD_BASE_DIR=$(BUILD_BASE_DIR) \\"; \
	  echo "	     SHELL=$(SHELL)"; \
	  echo "") >> $(LIB_DEP_FILE)
