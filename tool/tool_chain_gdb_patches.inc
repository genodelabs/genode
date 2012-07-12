$(CONTRIB_DIR)/gdb-$(GDB_VERSION)/configure:: $(CONTRIB_DIR)/gdb-$(GDB_VERSION)
	$(ECHO) "$(BRIGHT_COL)patching gdb...$(DEFAULT_COL)"
	@#
	@# Include 'solib.o' and 'solib-svr4.o' in arm*-*-* (non-OS) target configuration for shared library support
	@#
	$(VERBOSE)sed -i "s/gdb_target_obs=\"arm-tdep\.o\"$$/gdb_target_obs=\"arm-tdep.o solib.o solib-svr4.o\"/" $(CONTRIB_DIR)/gdb-$(GDB_VERSION)/gdb/configure.tgt
	@#
	@# Add an x86_64-*-* (non-OS) target with shared library support
	@#
	$(VERBOSE)sed -i "/^xtensa\*-\*-linux\*/ s/^/x86_64-*-*)\n\tgdb_target_obs=\"amd64-tdep.o i386-tdep.o i387-tdep.o solib.o solib-svr4.o\"\n\t;;\n/" $(CONTRIB_DIR)/gdb-$(GDB_VERSION)/gdb/configure.tgt
	@#
	@# Enable shared library support
	@#
	$(VERBOSE)sed -i "/^#include \"features\/i386\/amd64-avx\.c\"$$/ s/$$/\n\n#include \"solib-svr4\.h\"/" $(CONTRIB_DIR)/gdb-$(GDB_VERSION)/gdb/amd64-tdep.c
	$(VERBOSE)sed -i "/AMD64 generally uses/ s/^/  set_solib_svr4_fetch_link_map_offsets(gdbarch, svr4_lp64_fetch_link_map_offsets);\n\n/" $(CONTRIB_DIR)/gdb-$(GDB_VERSION)/gdb/amd64-tdep.c
	$(VERBOSE)sed -i "/^#include \"features\/arm-with-m\.c\"$$/ s/$$/\n\n#include \"solib-svr4\.h\"/" $(CONTRIB_DIR)/gdb-$(GDB_VERSION)/gdb/arm-tdep.c
	$(VERBOSE)sed -i "/gdbarch = gdbarch_alloc (&info, tdep);/ s/$$/\n\n  set_solib_svr4_fetch_link_map_offsets(gdbarch, svr4_ilp32_fetch_link_map_offsets);/" $(CONTRIB_DIR)/gdb-$(GDB_VERSION)/gdb/arm-tdep.c
	$(VERBOSE)sed -i "/^#include \"features\/i386\/i386-mmx\.c\"$$/ s/$$/\n\n#include \"solib-svr4\.h\"/" $(CONTRIB_DIR)/gdb-$(GDB_VERSION)/gdb/i386-tdep.c
	$(VERBOSE)sed -i "/gdbarch = gdbarch_alloc (&info, tdep);/ s/$$/\n\n  set_solib_svr4_fetch_link_map_offsets(gdbarch, svr4_ilp32_fetch_link_map_offsets);/" $(CONTRIB_DIR)/gdb-$(GDB_VERSION)/gdb/i386-tdep.c
	@#
	@# Enable software single-stepping on ARM
	@#
	$(VERBOSE)sed -i "/gdbarch = gdbarch_alloc (&info, tdep);/ s/$$/\n\n  set_gdbarch_software_single_step(gdbarch, arm_software_single_step);/" $(CONTRIB_DIR)/gdb-$(GDB_VERSION)/gdb/arm-tdep.c
