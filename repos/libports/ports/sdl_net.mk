include ports/sdl_net.inc

SDL_NET_TGZ = $(SDL_NET).tar.gz
SDL_NET_URL = http://www.libsdl.org/projects/SDL_net/release/$(SDL_NET_TGZ)

#
# Interface to top-level prepare Makefile
#
# Register SDL_net port as lower case to be consistent with the
# other libraries.
#
PORTS += sdl_net-$(SDL_NET_VERSION)

prepare-sdl_net: $(CONTRIB_DIR)/$(SDL_NET) include/SDL/SDL_net.h

$(CONTRIB_DIR)/$(SDL_NET): clean-sdl_net

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(SDL_NET_TGZ):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(SDL_NET_URL) && touch $@

$(CONTRIB_DIR)/$(SDL_NET): $(DOWNLOAD_DIR)/$(SDL_NET_TGZ)
	$(VERBOSE)tar xfz $< \
	          --exclude Xcode --exclude VisualC --exclude Xcode-iOS \
	          --exclude VisualCE --exclude Watcom-OS2.zip --exclude debian \
	          -C $(CONTRIB_DIR) && touch $@
	$(VERBOSE)patch -N -p0 < src/lib/sdl_net/SDLnet.patch
	$(VERBOSE)patch -N -p0 < src/lib/sdl_net/SDL_net.h.patch

#
# Install SDL_net headers
#
include/SDL/SDL_net.h:
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)ln -fs ../../$(CONTRIB_DIR)/$(SDL_NET)/SDL_net.h include/SDL/

clean-sdl_net:
	$(VERBOSE)rm -rf include/SDL/SDL_net.h
	$(VERBOSE)rmdir include/SDL 2>/dev/null || true
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(SDL_NET)
