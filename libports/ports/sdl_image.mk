include ports/sdl_image.inc

SDL_IMAGE_TGZ = $(SDL_IMAGE).tar.gz
SDL_IMAGE_URL = http://www.libsdl.org/projects/SDL_image/release/$(SDL_IMAGE_TGZ)

#
# Interface to top-level prepare Makefile
#
# Register SDL_image port as lower case to be consistent with the
# other libraries.
#
PORTS += sdl_image-$(SDL_IMAGE_VERSION)

prepare-sdl_image: $(CONTRIB_DIR)/$(SDL_IMAGE) include/SDL/SDL_image.h

$(CONTRIB_DIR)/$(SDL_IMAGE): clean-sdl_image

#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(SDL_IMAGE_TGZ):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(SDL_IMAGE_URL) && touch $@

$(CONTRIB_DIR)/$(SDL_IMAGE): $(DOWNLOAD_DIR)/$(SDL_IMAGE_TGZ)
	$(VERBOSE)tar xfz $< \
	          --exclude Xcode --exclude VisualC --exclude Xcode-iOS \
	          --exclude VisualCE --exclude Watcom-OS2.zip \
	          -C $(CONTRIB_DIR) && touch $@

#
# Install SDL_image headers
#
include/SDL/SDL_image.h:
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)ln -fs ../../$(CONTRIB_DIR)/$(SDL_IMAGE)/SDL_image.h include/SDL/

clean-sdl_image:
	$(VERBOSE)rm -rf include/SDL/SDL_image.h
	$(VERBOSE)rmdir include/SDL 2>/dev/null || true
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(SDL_IMAGE)
