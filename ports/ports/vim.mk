VIM      = vim-7.3
VIM_TBZ2 = $(VIM).tar.bz2
VIM_URL  = ftp://ftp.vim.org/pub/vim/unix/$(VIM_TBZ2)

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

$(CONTRIB_DIR)/$(VIM): $(DOWNLOAD_DIR)/$(VIM_TBZ2)
	$(VERBOSE)tar xfj $< -C $(CONTRIB_DIR)
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

