VIM      = vim-7.3
VIM_TBZ2 = $(VIM).tar.bz2
VIM_URL  = ftp://ftp.vim.org/pub/vim/unix/$(VIM_TBZ2)
# from ftp://ftp.vim.org/pub/vim/unix/MD5SUMS
VIM_MD5  = 5b9510a17074e2b37d8bb38ae09edbf2
#
# Interface to top-level prepare Makefile
#
PORTS += $(VIM)

#
# Check for tools
#
$(call check_tool,sed)

prepare:: $(CONTRIB_DIR)/$(VIM)

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(VIM_TBZ2):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(VIM_URL) && touch $@

$(DOWNLOAD_DIR)/$(VIM_TBZ2).verified: $(DOWNLOAD_DIR)/$(VIM_TBZ2)
	$(VERBOSE)$(HASHVERIFIER) $(DOWNLOAD_DIR)/$(VIM_TBZ2) $(VIM_MD5) md5
	$(VERBOSE)touch $@

$(CONTRIB_DIR)/$(VIM): $(DOWNLOAD_DIR)/$(VIM_TBZ2).verified
	$(VERBOSE)tar xfj $(<:.verified=) -C $(CONTRIB_DIR)
	$(VERBOSE)mv $(CONTRIB_DIR)/vim73 $@ && touch $@
	@#
	@# Prevent configure script from breaking unconditionally
	@# because of cross compiling.
	@#
	$(VERBOSE)sed -i "/could not compile program using uint32_t/s/^/#/" $@/src/auto/configure
	@#
	@# Fix compiled-in VIM install location. Otherwise, the absolute path used
	@# during the build will be compiled-in, which makes no sense in the Noux
	@# environment
	@#
	$(VERBOSE)sed -i "/default_vim_dir/s/.(VIMRCLOC)/\/share\/vim/" $@/src/Makefile
	$(VERBOSE)patch -d $(CONTRIB_DIR)/$(VIM) -N -p1 < src/noux-pkg/vim/build.patch
