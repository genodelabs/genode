include $(REP_DIR)/lib/mk/mesa.inc

MESA_SUBDIRS = main math vbo shader shader/slang glapi

# collect all source codes in 'MESA_SUBDIRS'
SRC_C := $(foreach subdir,$(MESA_SUBDIRS),$(wildcard $(MESA_SRC_DIR)/$(subdir)/*.c))

# prevent definition conflicts in lex.yy.c with libc
CC_OPT_lex_yy += -DFLEXINT_H -include inttypes.h -Dflex_int32_t=int32_t -Dflex_int16_t=int16_t

# dim warning noise for compiling contrib code
CC_OPT_bufferobj        += -Wno-unused-but-set-variable
CC_OPT_dlist            += -Wno-unused-but-set-variable
CC_OPT_glapi            += -Wno-strict-aliasing
CC_OPT_lex_yy           += -Wno-unused-function
CC_OPT_prog_print       += -Wno-format
CC_OPT_program          += -Wno-enum-compare
CC_OPT_shader_api       += -Wno-unused-but-set-variable
CC_OPT_slang_emit       += -Wno-unused-but-set-variable
CC_OPT_st_cb_texture    += -Wno-strict-aliasing
CC_OPT_texcompress_s3tc += -Wno-unused-but-set-variable
CC_OPT_varray           += -Wno-format

# glsl library
GLSL_SRC_DIR = $(MESA_PORT_DIR)/src/lib/mesa/src/glsl
GLSL_SUBDIRS = pp cl
SRC_C += $(foreach subdir,$(GLSL_SUBDIRS),$(wildcard $(GLSL_SRC_DIR)/$(subdir)/*.c))

# strip leading directories - keep only the file names
SRC_C := $(notdir $(SRC_C))

# remove non-needed files from list
SRC_C := $(filter-out vsnprintf.c,$(SRC_C))

vpath %.c $(addprefix $(MESA_SRC_DIR)/,$(MESA_SUBDIRS))
vpath %.c $(addprefix $(GLSL_SRC_DIR)/,$(GLSL_SUBDIRS))

#
# Compile built-in fragment and vertex shaders
#
# The shader programs are compiled to header files in a
# 'library/' subdirectory, which are then included by mesa's
# 'shader/slang' module.
#
SRC_GC   := $(wildcard $(MESA_SRC_DIR)/shader/slang/library/*.gc)
GEN_GC_H := $(notdir $(SRC_GC:.gc=_gc.h))

# make sure that the shaders are compiled prior the mesa source codes
$(SRC_C:.c=.o): $(addprefix library/,$(GEN_GC_H))

$(addprefix library/,$(GEN_GC_H)): library

library:
	$(VERBOSE)mkdir -p $@

library/%_gc.h: %.gc
	$(MSG_CONVERT)$@
	$(VERBOSE)$(BUILD_BASE_DIR)/tool/mesa/glsl/compiler fragment $< $@

library/slang_vertex_builtin_gc.h: slang_vertex_builtin.gc
	$(MSG_CONVERT)$@
	$(VERBOSE)$(BUILD_BASE_DIR)/tool/mesa/glsl/compiler vertex $< $@

vpath %.gc $(MESA_SRC_DIR)/shader/slang/library

