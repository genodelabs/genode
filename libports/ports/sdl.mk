include ports/sdl.inc

SDL_TGZ      = $(SDL).tar.gz
SDL_SIG      = $(SDL_TGZ).sig
SDL_BASE_URL = http://www.libsdl.org/release
SDL_URL      = $(SDL_BASE_URL)/$(SDL_TGZ)
SDL_URL_SIG  = $(SDL_BASE_URL)/$(SDL_SIG)
SDL_KEY      = 1528635D8053A57F77D1E08630A59377A7763BE6

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

$(DOWNLOAD_DIR)/$(SDL_SIG):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(SDL_URL_SIG) && touch $@

$(DOWNLOAD_DIR)/$(SDL_TGZ).verified: $(DOWNLOAD_DIR)/$(SDL_TGZ) \
                                     $(DOWNLOAD_DIR)/$(SDL_SIG)
	$(VERBOSE)$(SIGVERIFIER) $(DOWNLOAD_DIR)/$(SDL_TGZ) $(DOWNLOAD_DIR)/$(SDL_SIG) $(SDL_KEY)
	$(VERBOSE)touch $@

$(CONTRIB_DIR)/$(SDL): $(DOWNLOAD_DIR)/$(SDL_TGZ).verified
	$(VERBOSE)tar xfz $(<:.verified=) -C $(CONTRIB_DIR) && touch $@
	$(VERBOSE)rm -f $@/include/SDL_config.h
	$(VERBOSE)patch -p0 -i src/lib/sdl/SDL_video.patch
	$(VERBOSE)patch -d $(CONTRIB_DIR)/$(SDL) -p1 -i $(CURDIR)/src/lib/sdl/SDL_audio.patch

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
	$(VERBOSE)test -d $(CONTRIB_DIR)/$(SDL) && \
		for i in `find $(CONTRIB_DIR)/$(SDL)/include -name "*.h"`; do \
		rm -f include/SDL/`basename $$i`; done || true
	$(VERBOSE)rm -f $(addprefix include/SDL/,SDL_config_genode.h SDL_config.h)
	$(VERBOSE)rmdir include/SDL 2>/dev/null || true
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(SDL)
