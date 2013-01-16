ARORA     = arora-0.11.0
ARORA_TGZ = $(ARORA).tar.gz
ARORA_URL = http://arora.googlecode.com/files/$(ARORA_TGZ)

#
# Interface to top-level prepare Makefile
#
PORTS += $(ARORA)

prepare:: $(CONTRIB_DIR)/$(ARORA)

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(ARORA_TGZ):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(ARORA_URL) && touch $@

$(CONTRIB_DIR)/$(ARORA): $(DOWNLOAD_DIR)/$(ARORA_TGZ)
	$(VERBOSE)tar xfz $< -C $(CONTRIB_DIR) && touch $@
	$(VERBOSE)patch -d $@ -p1 -i ../../src/app/arora/arora_genode.patch
	$(VERBOSE)patch -d $@ -p1 -i ../../src/app/arora/arora_nitpicker_plugin.patch
	$(VERBOSE)patch -d $@ -p1 -i ../../src/app/arora/arora_move_window.patch
	$(VERBOSE)patch -d $@ -p1 -i ../../src/app/arora/arora_disable_adblock.patch
	$(VERBOSE)patch -d $@ -p1 -i ../../src/app/arora/arora_bookmarks.patch
	$(VERBOSE)patch -d $@ -p1 -i ../../src/app/arora/arora_disable_program_exit.patch
	$(VERBOSE)patch -d $@ -p1 -i ../../src/app/arora/arora_startpage.patch
	$(VERBOSE)patch -d $@ -p1 -i ../../src/app/arora/arora_disable_log_messages.patch
