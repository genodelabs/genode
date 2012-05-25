#
# \brief  Download and prepare Qt4 source code
# \author Christian Prochaska
# \author Norman Feske
# \date   2009-05-11
#

VERBOSE     ?= @
ECHO         = @echo
DOWNLOAD_DIR = download
CONTRIB_DIR  = contrib
PATCHES_DIR  = ../../src/lib/qt4

REP_DIR := $(realpath .)

include $(REP_DIR)/lib/mk/qt_version.inc

QT4_URL = ftp://ftp.trolltech.com/qt/source
QT4_TGZ = $(QT4).tar.gz

QTSCRIPTCLASSIC_URL = http://get.qt.nokia.com/qt/solutions/lgpl
QTSCRIPTCLASSIC     = qtscriptclassic-1.0_1-opensource
QTSCRIPTCLASSIC_TGZ = $(QTSCRIPTCLASSIC).tar.gz

PATCHES = qt4_genode.patch \
          qt4_nonblocking_sockets.patch \
          qt4_no_search_for_resolv_lib.patch \
          qt4_lwip_connect_semantics_adaption.patch \
          previewer_example.patch \
          textedit_example.patch \
          qt4_no_exit_on_window_close.patch \
          qt4_virtual_deletelater.patch \
          qt4_renderwidget.patch \
          qt4_include_time_h.patch

#
# Utility to check if a tool is installed
#
check_tool = $(if $(shell which $(1)),,$(error Need to have '$(1)' installed.))

$(call check_tool,wget)
$(call check_tool,patch)

help:
	$(ECHO)
	$(ECHO) "Prepare the Qt4 repository"
	$(ECHO)
	$(ECHO) "--- available commands ---"
	$(ECHO) "prepare  - download and extract the Qt4 source code archive and build the host tools"
	$(ECHO) "clean    - clean everything except downloaded archives"
	$(ECHO) "cleanall - clean everything including downloaded archives"
	$(ECHO)

prepare: $(CONTRIB_DIR)/$(QT4) $(CONTRIB_DIR)/$(QTSCRIPTCLASSIC) tools

$(CONTRIB_DIR)/$(QT4): $(CONTRIB_DIR) $(DOWNLOAD_DIR)/$(QT4_TGZ)
	$(VERBOSE)tar xzf $(DOWNLOAD_DIR)/$(QT4_TGZ) -C $(CONTRIB_DIR)
	$(VERBOSE)touch $(CONTRIB_DIR)/$(QT4)
	$(VERBOSE)for p in $(PATCHES); do \
	            patch -d $(CONTRIB_DIR)/$(QT4) -p1 -i $(PATCHES_DIR)/$$p; done

$(CONTRIB_DIR)/$(QTSCRIPTCLASSIC): $(CONTRIB_DIR) $(DOWNLOAD_DIR)/$(QTSCRIPTCLASSIC_TGZ)
	$(VERBOSE)tar xzf $(DOWNLOAD_DIR)/$(QTSCRIPTCLASSIC_TGZ) -C $(CONTRIB_DIR)
	$(VERBOSE)touch $(CONTRIB_DIR)/$(QTSCRIPTCLASSIC)

$(CONTRIB_DIR):
	$(VERBOSE)mkdir -p $(CONTRIB_DIR)

$(DOWNLOAD_DIR)/$(QT4_TGZ): $(DOWNLOAD_DIR)
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(QT4_URL)/$(QT4_TGZ) && touch $@

$(DOWNLOAD_DIR)/$(QTSCRIPTCLASSIC_TGZ): $(DOWNLOAD_DIR)
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(QTSCRIPTCLASSIC_URL)/$(QTSCRIPTCLASSIC_TGZ) && touch $@

$(DOWNLOAD_DIR):
	$(VERBOSE)mkdir -p $(DOWNLOAD_DIR)

tools:
	$(VERBOSE)make -C tool

clean:
	$(VERBOSE)make -C tool clean
	$(VERBOSE)rm -rf $(CONTRIB_DIR)

cleanall: clean
	$(VERBOSE)make -C tool distclean
	$(VERBOSE)rm -rf $(DOWNLOAD_DIR)
