TARGET   = scout
LIBS     = libpng_static libz_static mini_c launchpad scout_widgets
SRC_CC   = main.cc            doc.cc       \
           browser_window.cc  png_image.cc \
           navbar.cc          about.cc     \
           launcher.cc

CC_OPT  += -DPNG_USER_CONFIG

vpath %    $(PRG_DIR)/data
vpath %.cc $(PRG_DIR)

SRC_BIN +=    cover.rgba \
            forward.rgba \
           backward.rgba \
               home.rgba \
              index.rgba \
              about.rgba \
            pointer.rgba \
           nav_next.rgba \
           nav_prev.rgba

SRC_BIN += ior.map

#
# Browser content
#
CONTENT_IMAGES = launchpad.png liquid_fb_small.png x-ray_small.png \
                 setup.png genode_logo.png

SRC_BIN += $(addprefix $(REP_DIR)/doc/img/, $(CONTENT_IMAGES))

vpath % $(REP_DIR)/doc/img
