LIBS = libc libdrm

include $(REP_DIR)/lib/mk/mesa-common.inc

CC_OPT += -DHAVE_LIBDRM

INC_DIR += $(MESA_SRC_DIR)/src/compiler/nir \
           $(MESA_SRC_DIR)/src/etnaviv \
           $(MESA_SRC_DIR)/src/gallium/auxiliary \
           $(MESA_SRC_DIR)/src/gallium/drivers \
           $(MESA_SRC_DIR)/src/util \
           $(MESA_GEN_DIR)/src/compiler/nir \
           $(MESA_PORT_DIR)/include/drm-uapi

REP_INC_DIR += include/drm-uapi

SRC_C = etnaviv/drm/etnaviv_bo.c \
        etnaviv/drm/etnaviv_bo_cache.c \
        etnaviv/drm/etnaviv_cmd_stream.c \
        etnaviv/drm/etnaviv_device.c \
        etnaviv/drm/etnaviv_gpu.c \
        etnaviv/drm/etnaviv_perfmon.c \
        etnaviv/drm/etnaviv_pipe.c \
        gallium/drivers/etnaviv/etnaviv_asm.c \
        gallium/drivers/etnaviv/etnaviv_blend.c \
        gallium/drivers/etnaviv/etnaviv_blt.c \
        gallium/drivers/etnaviv/etnaviv_clear_blit.c \
        gallium/drivers/etnaviv/etnaviv_compiler.c \
        gallium/drivers/etnaviv/etnaviv_compiler_nir.c \
        gallium/drivers/etnaviv/etnaviv_compiler_nir_emit.c \
        gallium/drivers/etnaviv/etnaviv_compiler_nir_liveness.c \
        gallium/drivers/etnaviv/etnaviv_compiler_nir_ra.c \
        gallium/drivers/etnaviv/etnaviv_context.c \
        gallium/drivers/etnaviv/etnaviv_disasm.c \
        gallium/drivers/etnaviv/etnaviv_disk_cache.c \
        gallium/drivers/etnaviv/etnaviv_emit.c \
        gallium/drivers/etnaviv/etnaviv_etc2.c \
        gallium/drivers/etnaviv/etnaviv_fence.c \
        gallium/drivers/etnaviv/etnaviv_format.c \
        gallium/drivers/etnaviv/etnaviv_nir.c \
        gallium/drivers/etnaviv/etnaviv_nir_lower_source_mods.c \
        gallium/drivers/etnaviv/etnaviv_nir_lower_texture.c \
        gallium/drivers/etnaviv/etnaviv_nir_lower_ubo_to_uniform.c \
        gallium/drivers/etnaviv/etnaviv_perfmon.c \
        gallium/drivers/etnaviv/etnaviv_query.c \
        gallium/drivers/etnaviv/etnaviv_query_acc.c \
        gallium/drivers/etnaviv/etnaviv_query_acc_occlusion.c \
        gallium/drivers/etnaviv/etnaviv_query_acc_perfmon.c \
        gallium/drivers/etnaviv/etnaviv_query_sw.c \
        gallium/drivers/etnaviv/etnaviv_rasterizer.c \
        gallium/drivers/etnaviv/etnaviv_resource.c \
        gallium/drivers/etnaviv/etnaviv_rs.c \
        gallium/drivers/etnaviv/etnaviv_screen.c \
        gallium/drivers/etnaviv/etnaviv_shader.c \
        gallium/drivers/etnaviv/etnaviv_state.c \
        gallium/drivers/etnaviv/etnaviv_surface.c \
        gallium/drivers/etnaviv/etnaviv_texture.c \
        gallium/drivers/etnaviv/etnaviv_texture_desc.c \
        gallium/drivers/etnaviv/etnaviv_texture_state.c \
        gallium/drivers/etnaviv/etnaviv_tiling.c \
        gallium/drivers/etnaviv/etnaviv_transfer.c \
        gallium/drivers/etnaviv/etnaviv_uniforms.c \
        gallium/drivers/etnaviv/etnaviv_zsa.c \
        gallium/winsys/etnaviv/drm/etnaviv_drm_winsys.c \

vpath %.c $(MESA_SRC_DIR)/src
