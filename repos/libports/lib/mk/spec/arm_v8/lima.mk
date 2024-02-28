LIBS = libc libdrm

include $(REP_DIR)/lib/mk/mesa-common.inc

CC_OPT += -DHAVE_LIBDRM

INC_DIR += $(MESA_SRC_DIR)/src/compiler/nir \
           $(MESA_SRC_DIR)/src/gallium/auxiliary \
           $(MESA_SRC_DIR)/src/gallium/drivers \
           $(MESA_SRC_DIR)/src/gallium/drivers/lima \
           $(MESA_SRC_DIR)/src/panfrost/shared \
           $(MESA_SRC_DIR)/src/util \
           $(MESA_GEN_DIR)/src/compiler/nir \
           $(MESA_PORT_DIR)/include/drm-uapi

REP_INC_DIR += include/drm-uapi

SRC_C = gallium/drivers/lima/ir/gp/codegen.c \
        gallium/drivers/lima/ir/gp/disasm.c \
        gallium/drivers/lima/ir/gp/instr.c \
        gallium/drivers/lima/ir/gp/lower.c \
        gallium/drivers/lima/ir/gp/nir.c \
        gallium/drivers/lima/ir/gp/node.c \
        gallium/drivers/lima/ir/gp/optimize.c \
        gallium/drivers/lima/ir/gp/reduce_scheduler.c \
        gallium/drivers/lima/ir/gp/regalloc.c \
        gallium/drivers/lima/ir/gp/scheduler.c \
        gallium/drivers/lima/ir/lima_nir_duplicate_consts.c \
        gallium/drivers/lima/ir/lima_nir_duplicate_intrinsic.c \
        gallium/drivers/lima/ir/lima_nir_lower_uniform_to_scalar.c \
        gallium/drivers/lima/ir/lima_nir_lower_txp.c \
        gallium/drivers/lima/ir/lima_nir_split_load_input.c \
        gallium/drivers/lima/ir/lima_nir_split_loads.c \
        gallium/drivers/lima/ir/pp/codegen.c \
        gallium/drivers/lima/ir/pp/disasm.c \
        gallium/drivers/lima/ir/pp/instr.c \
        gallium/drivers/lima/ir/pp/liveness.c \
        gallium/drivers/lima/ir/pp/lower.c \
        gallium/drivers/lima/ir/pp/nir.c \
        gallium/drivers/lima/ir/pp/node.c \
        gallium/drivers/lima/ir/pp/node_to_instr.c \
        gallium/drivers/lima/ir/pp/regalloc.c \
        gallium/drivers/lima/ir/pp/scheduler.c \
        gallium/drivers/lima/lima_blit.c \
        gallium/drivers/lima/lima_bo.c \
        gallium/drivers/lima/lima_context.c \
        gallium/drivers/lima/lima_disk_cache.c \
        gallium/drivers/lima/lima_draw.c \
        gallium/drivers/lima/lima_fence.c \
        gallium/drivers/lima/lima_format.c \
        gallium/drivers/lima/lima_job.c \
        gallium/drivers/lima/lima_parser.c \
        gallium/drivers/lima/lima_program.c \
        gallium/drivers/lima/lima_query.c \
        gallium/drivers/lima/lima_resource.c \
        gallium/drivers/lima/lima_screen.c \
        gallium/drivers/lima/lima_state.c \
        gallium/drivers/lima/lima_texture.c \
        gallium/drivers/lima/lima_util.c \
        gallium/winsys/lima/drm/lima_drm_winsys.c \
        panfrost/shared/pan_minmax_cache.c \
        panfrost/shared/pan_tiling.c
#
# generated
# 
SRC_C += gallium/drivers/lima/lima_nir_algebraic.c

vpath %.c $(MESA_SRC_DIR)/src
vpath %.c $(MESA_SRC_DIR)/../../../generated/src
