LIBLZMA_PORT_DIR := $(call select_from_ports,xz)
LIBLZMA_DIR      := $(LIBLZMA_PORT_DIR)/src/xz/src/liblzma

CC_DEF += -DHAVE_CONFIG_H

SRC_C = $(notdir $(wildcard $(LIBLZMA_DIR)/common/*.c))
SRC_C += $(notdir $(wildcard $(LIBLZMA_DIR)/delta/*.c))
SRC_C += $(notdir $(wildcard $(LIBLZMA_DIR)/lz/*.c))
SRC_C += $(notdir $(wildcard $(LIBLZMA_DIR)/simple/*.c))

# tuklib
SRC_C += tuklib_physmem.c \
         tuklib_cpucores.c

# liblzma/check
SRC_C += check.c \
         crc32_fast.c \
         crc32_table.c \
         crc64_fast.c \
         crc64_table.c \
         sha256.c

# liblzma/lzma
SRC_C += fastpos_table.c \
         lzma_decoder.c \
         lzma_encoder_optimum_fast.c \
         lzma_encoder_optimum_normal.c \
         lzma_encoder_presets.c \
         lzma_encoder.c \
         lzma2_decoder.c \
         lzma2_encoder.c

# liblzma/rangecoder
SRC_C += price_table.c

vpath %.c $(LIBLZMA_DIR)/../common
vpath %.c $(LIBLZMA_DIR)/check
vpath %.c $(LIBLZMA_DIR)/common
vpath %.c $(LIBLZMA_DIR)/delta
vpath %.c $(LIBLZMA_DIR)/lz
vpath %.c $(LIBLZMA_DIR)/lzma
vpath %.c $(LIBLZMA_DIR)/rangecoder
vpath %.c $(LIBLZMA_DIR)/simple

# find 'config.h'
INC_DIR += $(REP_DIR)/src/lib/liblzma

INC_DIR += $(LIBLZMA_DIR)/../common \
           $(LIBLZMA_DIR)/api \
           $(LIBLZMA_DIR)/check \
           $(LIBLZMA_DIR)/common \
           $(LIBLZMA_DIR)/delta \
           $(LIBLZMA_DIR)/lz \
           $(LIBLZMA_DIR)/lzma \
           $(LIBLZMA_DIR)/rangecoder \
           $(LIBLZMA_DIR)/simple

LIBS += libc

SHARED_LIB = yes
