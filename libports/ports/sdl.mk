SDL_VERSION = 1.2.13
SDL         = SDL-$(SDL_VERSION)
SDL_TGZ     = $(SDL).tar.gz
SDL_URL     = http://www.libsdl.org/release/$(SDL_TGZ)

#
# Interface to top-level prepare Makefile
#
# Register SDL port as lower case to be consitent with the
# other libraries.
#
PORTS += sdl-$(SDL_VERSION)

prepare-sdl: $(CONTRIB_DIR)/$(SDL) include/SDL

$(CONTRIB_DIR)/$(SDL): clean-sdl

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(SDL_TGZ):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(SDL_URL) && touch $@

$(CONTRIB_DIR)/$(SDL): $(DOWNLOAD_DIR)/$(SDL_TGZ)
	$(VERBOSE)tar xfz $< -C $(CONTRIB_DIR) && touch $@
	$(VERBOSE)rm -f $@/include/SDL_config.h
	$(VERBOSE)patch -p0 -i src/lib/sdl/SDL_video.patch
	$(VERBOSE)patch -d $(CONTRIB_DIR)/$(SDL) -p1 -i ../../src/lib/sdl/SDL_audio.patch

#
# Install SDL headers
#
include/SDL:
	$(VERBOSE)mkdir -p $@
	$(VERBOSE)for i in `find $(CONTRIB_DIR)/$(SDL)/include -name *.h`; do \
		ln -fs ../../$$i include/SDL/; done
	$(VERBOSE)ln -fs ../../src/lib/sdl/SDL_config.h $@
	$(VERBOSE)ln -fs ../../src/lib/sdl/SDL_config_genode.h $@

clean-sdl:
	$(VERBOSE)rm -rf include/SDL
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(SDL)
