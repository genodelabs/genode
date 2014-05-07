include ports/sdl_mixer.inc

SDL_MIXER_TGZ = $(SDL_MIXER).tar.gz
SDL_MIXER_URL = http://www.libsdl.org/projects/SDL_mixer/release/$(SDL_MIXER_TGZ)

#
# Interface to top-level prepare Makefile
#
# Register SDL_mixer port as lower case to be consistent with the
# other libraries.
#
PORTS += sdl_mixer-$(SDL_MIXER_VERSION)

prepare-sdl_mixer: $(CONTRIB_DIR)/$(SDL_MIXER) include/SDL/SDL_mixer.h

$(CONTRIB_DIR)/$(SDL_MIXER): clean-sdl_mixer

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(SDL_MIXER_TGZ):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(SDL_MIXER_URL) && touch $@

$(CONTRIB_DIR)/$(SDL_MIXER): $(DOWNLOAD_DIR)/$(SDL_MIXER_TGZ)
	$(VERBOSE)tar xfz $< \
	          -C $(CONTRIB_DIR) && touch $@

#
# Install SDL_mixer headers
#
include/SDL/SDL_mixer.h:
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)ln -fs ../../$(CONTRIB_DIR)/$(SDL_MIXER)/SDL_mixer.h include/SDL/

clean-sdl_mixer:
	$(VERBOSE)rm -rf include/SDL/SDL_mixer.h
	$(VERBOSE)rmdir include/SDL 2>/dev/null || true
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(SDL_MIXER)
