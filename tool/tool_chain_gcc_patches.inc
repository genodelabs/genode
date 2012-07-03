$(CONTRIB_DIR)/gcc-$(GCC_VERSION)/configure:: $(CONTRIB_DIR)/gcc-$(GCC_VERSION)
	$(ECHO) "$(BRIGHT_COL)patching gcc build system...$(DEFAULT_COL)"
	@#
	@# Enable support for passing custom 'tmake_file' and 'extra_parts' to the
	@# GCC configure process uncommenting the default initialization of the
	@# respective variables. The 'extra_parts' variable is used to carry the
	@# the information about which crtN files are to be created.
	@#
	@# The 't-386elf' file must the treated to prevent it from defining the
	@# 'EXTRA_PARTS' variable. If defined, the 'libgcc' Makefile would prepare
	@# it against our custom list of 'extra_parts' and consequently fail.
	@#
	$(VERBOSE)sed -i "/^tmake_file=$$/s/^/#/" $(CONTRIB_DIR)/gcc-$(GCC_VERSION)/gcc/config.gcc
	$(VERBOSE)sed -i "/^extra_parts=$$/s/^/#/" $(CONTRIB_DIR)/gcc-$(GCC_VERSION)/gcc/config.gcc
	$(VERBOSE)sed -i "/^extra_parts=$$/s/^/#/" $(CONTRIB_DIR)/gcc-$(GCC_VERSION)/libgcc/config.host
	$(VERBOSE)sed -i "/^EXTRA_PARTS=/s/^/#/" $(CONTRIB_DIR)/gcc-$(GCC_VERSION)/gcc/config/i386/t-i386elf
	$(VERBOSE)sed -i "/^EXTRA_MULTILIB_PARTS *=/s/^/#/" $(CONTRIB_DIR)/gcc-$(GCC_VERSION)/gcc/config/arm/t-arm-elf
	@#
	@# Let 'config.gcc' expand our already populated 'tmake_file' variable rather
	@# than making a hard assignment. This is needed for the ARM platform because
	@# the target 'arm-elf-eabi' actually matches the pattern 'arm-*-*-eabi' in
	@# the 'config.gcc' file.
	@#
	$(VERBOSE)sed -i "/tmake_file=\"arm/s/=\"/=\"\$${tmake_file} /" $(CONTRIB_DIR)/gcc-$(GCC_VERSION)/gcc/config.gcc
	@#
	@# Enable LINK_SPEC customization via configure arguments
	@#
	@# We add a hook for sneaking our custom LINK_SPEC definition into the GCC
	@# configure process by uncommentig the 'host_xm_include_list'. This enables us
	@# to supply a custom header file to be included into 'gcc/config.h' defining
	@# the 'LINK_SPEC' macro. This macro expresses the policy of how the GCC
	@# frontend invokes 'ld' on multiarch platforms. I.e., on x86, we need to pass
	@# '-melf_i386' to 'ld' when building in '-m32' mode.
	@#
	$(VERBOSE)sed -i "/^host_xm_include_list=$$/s/^/#/" $(CONTRIB_DIR)/gcc-$(GCC_VERSION)/gcc/configure.ac
	@#
	@# Make sure to disable the 'inhibit_libc' flag, which is evaluated when
	@# compiling libgcc. When libc is inhibited, the 'unwind-dw2-fde-glibc.c'
	@# is not using the "new" glibc exception handling mechanism. However,
	@# Genode's dynamic linker relies on this mechanism.
	@#
	$(VERBOSE)sed -i "/inhibit_libc=true/s/true/false # was true/" $(CONTRIB_DIR)/gcc-$(GCC_VERSION)/gcc/configure.ac
	@#
	@# Accept prepopulation of 'host_configargs' and 'target_configargs' as
	@# configure argument (only needed for gcc-4.4.5, fixed with later versions)
	@#
	$(VERBOSE)sed -i "/host_configargs=.--cache-file/s/=./=\"\$$host_configargs /" $(CONTRIB_DIR)/gcc-$(GCC_VERSION)/configure.ac
	$(VERBOSE)sed -i "/target_configargs=..baseargs/s/=.*/=\"\$$target_configargs \$${baseargs}\"/" $(CONTRIB_DIR)/gcc-$(GCC_VERSION)/configure.ac
	@#
	@# Allow customization of CPPFLAGS_FOR_TARGET, not supported by the original
	@# GCC config and build system.
	@#
	$(VERBOSE)sed -i "/^CXXFLAGS_FOR_TARGET =/s/^/CPPFLAGS_FOR_TARGET = @CPPFLAGS_FOR_TARGET@\n/" $(CONTRIB_DIR)/gcc-$(GCC_VERSION)/Makefile.tpl
	$(VERBOSE)sed -i "/AC_SUBST(CFLAGS_FOR_TARGET)/s/^/AC_SUBST(CPPFLAGS_FOR_TARGET)\n/"          $(CONTRIB_DIR)/gcc-$(GCC_VERSION)/configure.ac
	@#
	@# Fix the handling of CPP_FOR_TARGET. Without the fix, the configure script
	@# of libgcc tries to use the normal 'cpp' for executing preprocessor tests,
	@# which produces bogus results.
	@#
	$(VERBOSE)sed -i "/CC=.*XGCC.*export CC/s/^/	CPP=\"\$$(CPP_FOR_TARGET) \$$(XGCC_FLAGS_FOR_TARGET) \$$\$$TFLAGS\"; export CPP; \\\\\n/"  $(CONTRIB_DIR)/gcc-$(GCC_VERSION)/Makefile.tpl
	$(VERBOSE)sed -i "/^CC_FOR_TARGET=/s/^/CPP_FOR_TARGET=\$$(STAGE_CC_WRAPPER) @CPP_FOR_TARGET@\n/"                                           $(CONTRIB_DIR)/gcc-$(GCC_VERSION)/Makefile.tpl
	$(VERBOSE)sed -i "/CC=.*XGCC.*TFLAGS....$$/s/^/	'CPP=\$$\$$(CPP_FOR_TARGET) \$$\$$(XGCC_FLAGS_FOR_TARGET) \$$\$$(TFLAGS)' \\\\\n/"         $(CONTRIB_DIR)/gcc-$(GCC_VERSION)/Makefile.tpl
	$(VERBOSE)sed -i "/flag= CC_FOR_TARGET/s/^/flags_to_pass = { flag= CPP_FOR_TARGET ; };\n/"                                                 $(CONTRIB_DIR)/gcc-$(GCC_VERSION)/Makefile.def
	$(VERBOSE)sed -i "/^GCC_TARGET_TOOL.cc,/s/^/GCC_TARGET_TOOL(cpp, CPP_FOR_TARGET, CPP, \[gcc\/cpp -B\$$\$$r\/\$$(HOST_SUBDIR)\/gcc\/\])\n/" $(CONTRIB_DIR)/gcc-$(GCC_VERSION)/configure.ac
	@#
	@# Ensure -fno-short-enums as default.
	@#
	$(VERBOSE)sed -i "s/return TARGET_AAPCS_BASED && arm_abi != ARM_ABI_AAPCS_LINUX/return false/" $(CONTRIB_DIR)/gcc-$(GCC_VERSION)/gcc/config/arm/arm.c
	@#
	@# Remove sanity check for host/target combination in configure script of
	@# libstdc++. An alternative fix would be the addition of a new host or
	@# the use of an existing one. However, adding a new host would require
	@# us to maintain a larger patch to the GCC build system, and using an
	@# existing host comes with all the (possibly unwanted) policies associated
	@# with the respective host platform. We want to stick with the bare-bone
	@# compiler as much as possible.
	@#
	$(VERBOSE)sed -i "/No support for this host.target combination/d" $(CONTRIB_DIR)/gcc-$(GCC_VERSION)/libstdc++-v3/crossconfig.m4
	$(VERBOSE)cd $(CONTRIB_DIR)/gcc-$(GCC_VERSION)/libstdc++-v3; $(AUTOCONF)
	@#
	@# Allow passing of PICFLAGS to the configure script of libstdc++.
	@# Without this change, libsupc++ would be compiled w/o PICFLAGS, resulting
	@# in text relocations. Because for base tool chains, no 'dynamic_linker'
	@# is defined (see 'libtool.m4'), 'dynamic_linker' is set to 'no', which
	@# results in 'can_build_shared = no', which, in turn , results
	@# in 'enable_shared = no', which, in turn, sets 'LIBSUPCXX_PICFLAGS' to
	@# nothing rather then '-prefer-pic'.
	@#
	$(VERBOSE)sed -i "/LIBSUPCXX_PICFLAGS=$$/s/LIB/__LIB/" $(CONTRIB_DIR)/gcc-$(GCC_VERSION)/libstdc++-v3/configure.ac
	@#
	@# Re-generate configure scripts
	@#
	$(VERBOSE)cd $(CONTRIB_DIR)/gcc-$(GCC_VERSION); autogen Makefile.def
	$(VERBOSE)cd $(CONTRIB_DIR)/gcc-$(GCC_VERSION)/libgcc; $(AUTOCONF)
	$(VERBOSE)cd $(CONTRIB_DIR)/gcc-$(GCC_VERSION)/libstdc++-v3; $(AUTOCONF)
	$(VERBOSE)cd $(CONTRIB_DIR)/gcc-$(GCC_VERSION)/gcc; $(AUTOCONF)
	$(VERBOSE)cd $(CONTRIB_DIR)/gcc-$(GCC_VERSION); $(AUTOCONF)
	@#
	@# Fix a bug in gcc 4.6.1 that causes compile errors when building Qt4 for ARM
	@# More detailed description at and solution from http://gcc.gnu.org/ml/gcc-patches/2010-11/msg02245.html
	@#
ifeq ($(GCC_VERSION),4.6.1)
	$(ECHO) "$(BRIGHT_COL)patching gcc...$(DEFAULT_COL)"
	$(VERBOSE)sed -i "/|| (volatilep && flag_strict_volatile_bitfields > 0/s/)/ \&\& (bitpos % GET_MODE_ALIGNMENT (mode) != 0))/" $(CONTRIB_DIR)/gcc-$(GCC_VERSION)/gcc/expr.c
endif
