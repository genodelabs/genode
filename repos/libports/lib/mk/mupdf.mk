MUPDF     = mupdf-0.9
MUPDF_DIR = $(REP_DIR)/contrib/$(MUPDF)
LIBS     += libc jpeg zlib jbig2dec openjpeg freetype
INC_DIR  += $(addprefix $(MUPDF_DIR)/,fitz pdf xps)

SRC_C  = $(addprefix fitz/,$(notdir $(wildcard $(MUPDF_DIR)/fitz/*.c)))
SRC_C += $(addprefix pdf/, $(notdir $(wildcard $(MUPDF_DIR)/pdf/*.c)))
SRC_C += $(addprefix xps/, $(notdir $(wildcard $(MUPDF_DIR)/xps/*.c)))
SRC_C += $(addprefix draw/,$(notdir $(wildcard $(MUPDF_DIR)/draw/*.c)))

# disable warning noise for contrib code
CC_WARN += -Wall -Wno-uninitialized -Wno-unused-but-set-variable

vpath %.c $(MUPDF_DIR)

SHARED_LIB = yes
