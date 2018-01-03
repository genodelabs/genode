PORT_DIR     := $(call select_from_ports,freetype)
FREETYPE_DIR := $(PORT_DIR)/src/lib/freetype/contrib
LIBS         += libc

# add local freetype headers to include-search path
INC_DIR += $(PORT_DIR)/include $(FREETYPE_DIR)/src/base

# use our custom freetype config files
CC_DEF += -DFT_CONFIG_CONFIG_H="<freetype-genode/ftconfig.h>"
CC_DEF += -DFT2_BUILD_LIBRARY
CC_DEF += -DFT_CONFIG_MODULES_H="<freetype-genode/ftmodule.h>"

# sources from freetype 'src/base/' directory
SRC_C = \
	ftbase.c ftbbox.c ftbdf.c ftbitmap.c ftcid.c ftdebug.c ftfstype.c \
	ftgasp.c ftglyph.c ftgxval.c ftinit.c ftlcdfil.c ftmm.c ftotval.c \
	ftpatent.c ftpfr.c ftstroke.c ftsynth.c fttype1.c ftwinfnt.c ftxf86.c \
	ftsystem.c

# sources from other freetype directories
SRC_C += \
	truetype.c type1.c cff.c type1cid.c pfr.c type42.c winfnt.c pcf.c bdf.c \
	sfnt.c autofit.c pshinter.c raster.c smooth.c ftcache.c ftgzip.c ftlzw.c \
	psaux.c psmodule.c

# dim build noise for contrib code
CC_OPT_autofit += -Wno-unused-but-set-variable
CC_OPT_cff     += -Wno-unused-but-set-variable

vpath %.c        $(FREETYPE_DIR)/src/base
vpath truetype.c $(FREETYPE_DIR)/src/truetype
vpath type1.c    $(FREETYPE_DIR)/src/type1
vpath cff.c      $(FREETYPE_DIR)/src/cff
vpath type1cid.c $(FREETYPE_DIR)/src/cid
vpath pfr.c      $(FREETYPE_DIR)/src/pfr
vpath type42.c   $(FREETYPE_DIR)/src/type42
vpath winfnt.c   $(FREETYPE_DIR)/src/winfonts
vpath pcf.c      $(FREETYPE_DIR)/src/pcf
vpath bdf.c      $(FREETYPE_DIR)/src/bdf
vpath sfnt.c     $(FREETYPE_DIR)/src/sfnt
vpath autofit.c  $(FREETYPE_DIR)/src/autofit
vpath pshinter.c $(FREETYPE_DIR)/src/pshinter
vpath raster.c   $(FREETYPE_DIR)/src/raster
vpath smooth.c   $(FREETYPE_DIR)/src/smooth
vpath ftcache.c  $(FREETYPE_DIR)/src/cache
vpath ftgzip.c   $(FREETYPE_DIR)/src/gzip
vpath ftlzw.c    $(FREETYPE_DIR)/src/lzw
vpath psaux.c    $(FREETYPE_DIR)/src/psaux
vpath psmodule.c $(FREETYPE_DIR)/src/psnames

SHARED_LIB = yes

CC_CXX_WARN_STRICT =
