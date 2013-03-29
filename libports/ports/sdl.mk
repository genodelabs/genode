include ports/sdl.inc

SDL_TGZ = $(SDL).tar.gz
SDL_URL = http://www.libsdl.org/release/$(SDL_TGZ)

#
# Interface to top-level prepare Makefile
#
# Register SDL port as lower case to be consistent with the
# other libraries.
#
PORTS += sdl-$(SDL_VERSION)

prepare-sdl: $(CONTRIB_DIR)/$(SDL) include/SDL/SDL_platform.h

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
# Use 'SDL_platform.h' as representative of the header files to be installed.
#
include/SDL/SDL_platform.h:
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)for i in `find $(CONTRIB_DIR)/$(SDL)/include -name "*.h"`; do \
		ln -fs ../../$$i include/SDL/; done
	$(VERBOSE)ln -fs ../../src/lib/sdl/SDL_config.h $(dir $@)
	$(VERBOSE)ln -fs ../../src/lib/sdl/SDL_config_genode.h $(dir $@)

clean-sdl:
	$(VERBOSE)for i in `find $(CONTRIB_DIR)/$(SDL)/include -name "*.h"`; do \
		rm -f include/SDL/`basename $$i`; done
	$(VERBOSE)rm -f $(addprefix include/SDL/,SDL_config_genode.h SDL_config.h)
	$(VERBOSE)rmdir include/SDL 2>/dev/null || true
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(SDL)
