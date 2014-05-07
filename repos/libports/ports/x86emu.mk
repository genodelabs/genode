# XXX We stay with 1.12.0 currently as later versions changed many places just cosmetically.
X86EMU_VERSION = 1.12.0
X86EMU         = x86emu-$(X86EMU_VERSION)
X86EMU_XSERVER = xserver-xorg-server-$(X86EMU_VERSION)
X86EMU_TGZ     = $(X86EMU_XSERVER).tar.gz
X86EMU_URL     = http://cgit.freedesktop.org/xorg/xserver/snapshot/$(X86EMU_TGZ)

#
# Check for tools
#
$(call check_tool,sed)

#
# Interface to top-level prepare Makefile
#
PORTS += $(X86EMU)

X86EMU_INC_DIR = include/x86emu

X86EMU_INCLUDES  = $(X86EMU_INC_DIR)/x86emu.h
X86EMU_INCLUDES += $(X86EMU_INC_DIR)/x86emu/regs.h
X86EMU_INCLUDES += $(X86EMU_INC_DIR)/x86emu/types.h

# dummy links
X86EMU_INCLUDES += $(addprefix $(X86EMU_INC_DIR)/, stdint.h stdio.h stdlib.h string.h) 

prepare-x86emu: $(CONTRIB_DIR)/$(X86EMU) $(X86EMU_INC_DIR)/x86emu $(X86EMU_INCLUDES)

$(CONTRIB_DIR)/$(X86EMU): clean-x86emu

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(X86EMU_TGZ):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(X86EMU_URL) && touch $@

# use sed to replace in a structure a member 'private' - reserved keyword in C++
$(CONTRIB_DIR)/$(X86EMU): $(DOWNLOAD_DIR)/$(X86EMU_TGZ)
	$(VERBOSE)mkdir $@ \
		&& tar xzf $< -C $@ --strip-components=4 $(X86EMU_XSERVER)/hw/xfree86/x86emu \
		&& cp $(CONTRIB_DIR)/$(X86EMU)/x86emu/regs.h $(CONTRIB_DIR)/$(X86EMU)/x86emu/regs.h.orig \
		&& sed 's/private;/private_ptr;/g' <$(CONTRIB_DIR)/$(X86EMU)/x86emu/regs.h.orig >$(CONTRIB_DIR)/$(X86EMU)/x86emu/regs.h \
		&& touch $@

$(X86EMU_INC_DIR)/x86emu:
	$(VERBOSE) mkdir $@

# create dummy links to std*.h
$(X86EMU_INC_DIR)/std%.h:
	$(VERBOSE) ln -sf sys/types.h $@

# create dummy link to string.h
$(X86EMU_INC_DIR)/string.h:
	$(VERBOSE) ln -sf sys/types.h $@

# create links to x86emu header files
$(X86EMU_INC_DIR)/%.h:
	$(VERBOSE)ln -sf ../..$(subst $(subst x86emu/,,$(subst $(X86EMU_INC_DIR)/,,$@)),,$(subst x86emu/,/..,$(subst $(X86EMU_INC_DIR)/,,$@)))/$(CONTRIB_DIR)/$(X86EMU)/$(subst $(X86EMU_INC_DIR)/,,$@) $@

clean-x86emu:
	$(VERBOSE)rm -f $(X86EMU_INCLUDES)
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(X86EMU)
