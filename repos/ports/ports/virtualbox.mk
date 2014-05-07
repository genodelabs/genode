include ports/virtualbox.inc

VIRTUALBOX      = virtualbox-4.2.16
VIRTUALBOX_TBZ2 = VirtualBox-4.2.16.tar.bz2
VIRTUALBOX_URL  = http://download.virtualbox.org/virtualbox/4.2.16/$(VIRTUALBOX_TBZ2)
VIRTUALBOX_MD5  = c4a36e2099a317f4715cd3861cdae238

VIRTUALBOX_CONTENT = src/VBox/VMM \
                     src/VBox/Devices \
                     src/VBox/Runtime \
                     src/VBox/GuestHost/HGSMI \
                     src/VBox/Frontends/VBoxBFE \
                     src/VBox/Storage \
                     src/VBox/Disassembler \
                     src/recompiler \
                     src/VBox/Main/include/MouseImpl.h \
                     src/VBox/Main/include/ConsoleEvents.h \
                     src/VBox/Main/src-client/MouseImpl.cpp \
                     src/libs/zlib-1.2.6 \
                     src/libs/liblzf-3.4 \
                     include/VBox/vmm \
                     include/iprt \
                     $(addprefix include/VBox/,types.h cdefs.h hgcmsvc.h \
                                               err.h dis.h disopcode.h \
                                               log.h sup.h pci.h param.h \
                                               ostypes.h VMMDev.h VMMDev2.h \
                                               vusb.h dbg.h version.h \
                                               VBoxVideo.h Hardware bioslogo.h \
                                               scsi.h HGSMI) \
                     include/VBox/msi.h \
                     include/VBox/DevPCNet.h \
                     include/VBox/asmdefs.mac \
                     include/VBox/err.mac \
                     include/VBox/vd.h \
                     include/VBox/vd-ifs.h \
                     include/VBox/vd-plugin.h \
                     include/VBox/vd-ifs-internal.h \
                     include/VBox/vd-cache-plugin.h

#
# Interface to top-level prepare Makefile
#
PORTS += $(VIRTUALBOX)
#
# Check for tools
#
$(call check_tool,iasl)
$(call check_tool,yasm)


PATCHES := $(shell find $(CURDIR)/src/virtualbox/ -name "*.patch")


apply_patches:
	$(VERBOSE)set -e; for p in $(PATCHES); do \
		echo $$p; \
		patch -p0 -N -d $(CONTRIB_DIR)/$(VIRTUALBOX) -i $$p; \
	done

prepare:: $(CONTRIB_DIR)/$(VIRTUALBOX) apply_patches

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(VIRTUALBOX_TBZ2):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(VIRTUALBOX_URL) && touch $@

$(DOWNLOAD_DIR)/$(VIRTUALBOX_SIG):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(VIRTUALBOX_URL_SIG) && touch $@

$(DOWNLOAD_DIR)/$(VIRTUALBOX_TBZ2).verified: $(DOWNLOAD_DIR)/$(VIRTUALBOX_TBZ2)
	$(VERBOSE)$(HASHVERIFIER) $(DOWNLOAD_DIR)/$(VIRTUALBOX_TBZ2) $(VIRTUALBOX_MD5) md5
	$(VERBOSE)touch $@

$(CONTRIB_DIR)/$(VIRTUALBOX): $(DOWNLOAD_DIR)/$(VIRTUALBOX_TBZ2).verified
	$(VERBOSE)tar xfj $(<:.verified=) \
		--transform "s/$(VIRTUALBOX_TBZ2:.tar.bz2=)/$(VIRTUALBOX)/" \
		-C $(CONTRIB_DIR) \
		$(addprefix $(VIRTUALBOX_TBZ2:.tar.bz2=)/,$(VIRTUALBOX_CONTENT)) && \
	rm $(CONTRIB_DIR)/$(VIRTUALBOX)/src/VBox/Frontends/VBoxBFE/SDLConsole.h && \
	rm $(CONTRIB_DIR)/$(VIRTUALBOX)/src/VBox/Frontends/VBoxBFE/SDLFramebuffer.h; \
	if [ $$? -ne 0 ]; then rm -r $(CONTRIB_DIR)/$(VIRTUALBOX); exit 1; fi
