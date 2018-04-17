MIRROR_FROM_REP_DIR := lib/symbols/sdl lib/import/import-sdl.mk \
                       lib/mk/mesa_api.mk lib/mk/sdlmain.mk

content: $(MIRROR_FROM_REP_DIR) src/lib/sdl include LICENSE

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

SDL_PORT_DIR  := $(call port_dir,$(REP_DIR)/ports/sdl)
MESA_PORT_DIR := $(call port_dir,$(REP_DIR)/ports/mesa)

#
# The Mesa header files needed for SDL_OpenGL are copied as well.
#
include:
	mkdir -p $@
	cp -r $(SDL_PORT_DIR)/include/SDL $@/
	cp -r $(REP_DIR)/include/SDL $@/
	cp -r $(MESA_PORT_DIR)/include/* $@/
	cp -r $(REP_DIR)/include/EGL $@/

src/lib/sdl:
	mkdir -p $@
	cp -r $(REP_DIR)/src/lib/sdl/sdl_main.cc $@/

LICENSE:
	cp $(SDL_PORT_DIR)/src/lib/sdl/COPYING $@
