#
# Create prerequisites for building Genode for Pistachio
#

#
# Execute the rules in this file only at the second build stage when we know
# about the complete build settings, e.g., the 'CROSS_DEV_PREFIX'.
#
ifeq ($(called_from_lib_mk),yes)

all: $(filter-out $(wildcard $(PISTACHIO_USER_BUILD_DIR)), $(PISTACHIO_USER_BUILD_DIR))

LD_PREFIX = "-Wl,"

$(PISTACHIO_USER_BUILD_DIR):
	$(VERBOSE)mkdir $@
	$(VERBOSE)cd $@; \
		LIBGCCFLAGS="$(CC_MARCH)" \
		LDFLAGS="$(addprefix $(LD_PREFIX),$(LD_MARCH)) -nostdlib" \
		CFLAGS="$(CC_MARCH)" \
		$(REP_DIR)/contrib/user/configure --build=ia32 --host i686 \
		                                  CC=$(CROSS_DEV_PREFIX)gcc
	$(VERBOSE_MK) MAKEFLAGS= $(MAKE) $(VERBOSE_DIR) -C $@

endif
