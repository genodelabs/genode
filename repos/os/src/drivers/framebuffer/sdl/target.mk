TARGET   = fb_sdl
LIBS     = lx_hybrid
REQUIRES = linux sdl
SRC_CC   = fb_sdl.cc input.cc
LX_LIBS  = sdl

#
# Explicitly add host headers to the include-search location. Even though this
# path happens to be added via the 'lx_hybrid' library via the 'HOST_INC_DIR'
# variable, we want to give /usr/include preference for resolving 'SDL/*'
# headers. Otherwise, if libSDL is prepared in 'libports', the build system
# would pull-in the SDL headers from libports.
#
INC_DIR += /usr/include

