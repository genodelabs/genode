include $(REP_DIR)/lib/mk/gallium.inc

GALLIUM_AUX_SRC_DIR = $(GALLIUM_SRC_DIR)/auxiliary

SUBDIRS = cso_cache draw indices os pipebuffer rbug rtasm tgsi translate util vl

# collect all source codes in 'SUBDIRS'
SRC_C := $(foreach subdir,$(SUBDIRS),$(wildcard $(GALLIUM_AUX_SRC_DIR)/$(subdir)/*.c))

# strip leading directories - keep only the file names
SRC_C := $(notdir $(SRC_C))

# add sources normally generated in 'indices' subdirectory
SRC_C += u_indices_gen.c u_unfilled_gen.c

# add sources normally generated in 'util' subdirectory
SRC_C += u_format_access.c u_format_table.c

# remove non-needed files from list
SRC_C := $(filter-out u_indices.c u_unfilled_indices.c u_debug_memory.c,$(SRC_C))

# dim build noise
CC_OPT_draw_vertex           += -Wno-unused-but-set-variable
CC_OPT_draw_vs_varient       += -Wno-enum-compare
CC_OPT_rbug_context          += -Wno-unused-but-set-variable
CC_OPT_rbug_core             += -Wno-unused-but-set-variable
CC_OPT_rbug_texture          += -Wno-unused-but-set-variable
CC_OPT_tgsi_build            += -Wno-uninitialized
CC_OPT_tgsi_build            += -Wno-unused-but-set-variable
CC_OPT_u_cpu_detect          += -Wno-pointer-sign
CC_OPT_u_debug_stack         += -Wno-unused-but-set-variable
CC_OPT_u_format_access       += -Wno-unused
CC_OPT_vl_mpeg12_mc_renderer += -Wno-enum-compare

PYTHON2 := $(VERBOSE)$(lastword $(shell which python2 python2.{4,5,6,7,8}))

u_%_gen.c: $(GALLIUM_SRC_DIR)/indices/u_%_gen.py
	$(MSG_CONVERT)$@
	$(PYTHON2) $< > $@

#
# To generate 'u_format_pack.h' as well, so we explicitly state that
# 'u_format_access.c' depends on it.
#
u_format_access.c: u_format_pack.h

u_format_%.c u_format_%.h: $(GALLIUM_AUX_SRC_DIR)/util/u_format_%.py
	$(MSG_CONVERT)$@
	$(PYTHON2) $< $(GALLIUM_AUX_SRC_DIR)/util/u_format.csv > $@

vpath %.c $(addprefix $(GALLIUM_AUX_SRC_DIR)/,$(SUBDIRS))
