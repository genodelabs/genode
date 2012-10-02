VANCOUVER_REV     = a7f4c2de42247e7a7c6ddb27a48f8a7d93d469ba
VANCOUVER         = vancouver-git
VANCOUVER_URL     = git://github.com/TUD-OS/NUL.git

#
# Check for tools
#
$(call check_tool,git)

#
# Interface to top-level prepare Makefile
#
PORTS += $(VANCOUVER)

prepare:: $(CONTRIB_DIR)/$(VANCOUVER)

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(VANCOUVER)/.git:
	$(VERBOSE)git clone $(VANCOUVER_URL) $(DOWNLOAD_DIR)/$(VANCOUVER) && \
	cd download/vancouver-git && \
	git reset --hard $(VANCOUVER_REV) && \
	cd ../.. && touch $@

$(CONTRIB_DIR)/$(VANCOUVER)/.git: $(DOWNLOAD_DIR)/$(VANCOUVER)/.git
	$(VERBOSE)git clone $(DOWNLOAD_DIR)/$(VANCOUVER) $(CONTRIB_DIR)/$(VANCOUVER)

$(CONTRIB_DIR)/$(VANCOUVER): $(CONTRIB_DIR)/$(VANCOUVER)/.git
