include ports/sdl_ttf.inc

SDL_TTF_TGZ = $(SDL_TTF).tar.gz
SDL_TTF_URL = http://www.libsdl.org/projects/SDL_ttf/release/$(SDL_TTF_TGZ)

#
# Interface to top-level prepare Makefile
#
# Register SDL_ttf port as lower case to be consistent with the
# other libraries.
#
PORTS += sdl_ttf-$(SDL_TTF_VERSION)

prepare-sdl_ttf: $(CONTRIB_DIR)/$(SDL_TTF) include/SDL/SDL_ttf.h

$(CONTRIB_DIR)/$(SDL_TTF): clean-sdl_ttf

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(SDL_TTF_TGZ):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(SDL_TTF_URL) && touch $@

$(CONTRIB_DIR)/$(SDL_TTF): $(DOWNLOAD_DIR)/$(SDL_TTF_TGZ)
	$(VERBOSE)tar xfz $< \
	          --exclude Xcode --exclude VisualC --exclude Xcode-iOS \
	          --exclude Watcom-Win32.zip \
	          -C $(CONTRIB_DIR) && touch $@

#
# Install SDL_ttf headers
#
include/SDL/SDL_ttf.h:
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)ln -fs ../../$(CONTRIB_DIR)/$(SDL_TTF)/SDL_ttf.h include/SDL/

clean-sdl_ttf:
	$(VERBOSE)rm -rf include/SDL/SDL_ttf.h
	$(VERBOSE)rmdir include/SDL 2>/dev/null || true
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(SDL_TTF)
