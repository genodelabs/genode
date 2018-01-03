LIBS       = libc drm

include $(REP_DIR)/lib/mk/mesa-common.inc

I965_COMPILER_FILES = \
	brw_compiler.c \
	brw_device_info.c \
	brw_disasm.c \
	brw_eu.c \
	brw_eu_compact.c \
	brw_eu_emit.c \
	brw_eu_util.c \
	brw_eu_validate.c \
	brw_interpolation_map.c \
	brw_nir.c \
	brw_nir_analyze_boolean_resolves.c \
	brw_nir_attribute_workarounds.c \
	brw_nir_opt_peephole_ffma.c \
	brw_packed_float.c \
	brw_surface_formats.c \
	brw_util.c \
	brw_vue_map.c \
	intel_asm_annotation.c \
	intel_debug.c \
	intel_resolve_map.c

I965_COMPILER_FILES_CXX = \
	brw_cfg.cpp \
	brw_dead_control_flow.cpp \
	brw_fs_cmod_propagation.cpp \
	brw_fs_combine_constants.cpp \
	brw_fs_copy_propagation.cpp \
	brw_fs.cpp \
	brw_fs_cse.cpp \
	brw_fs_dead_code_eliminate.cpp \
	brw_fs_generator.cpp \
	brw_fs_live_variables.cpp \
	brw_fs_nir.cpp \
	brw_fs_reg_allocate.cpp \
	brw_fs_register_coalesce.cpp \
	brw_fs_saturate_propagation.cpp \
	brw_fs_sel_peephole.cpp \
	brw_fs_surface_builder.cpp \
	brw_fs_validate.cpp \
	brw_fs_visitor.cpp \
	brw_nir_uniforms.cpp \
	brw_predicated_break.cpp \
	brw_schedule_instructions.cpp \
	brw_shader.cpp \
	brw_vec4_cmod_propagation.cpp \
	brw_vec4_copy_propagation.cpp \
	brw_vec4.cpp \
	brw_vec4_cse.cpp \
	brw_vec4_dead_code_eliminate.cpp \
	brw_vec4_generator.cpp \
	brw_vec4_gs_visitor.cpp \
	brw_vec4_live_variables.cpp \
	brw_vec4_nir.cpp \
	brw_vec4_gs_nir.cpp \
	brw_vec4_reg_allocate.cpp \
	brw_vec4_surface_builder.cpp \
	brw_vec4_tcs.cpp \
	brw_vec4_tes.cpp \
	brw_vec4_visitor.cpp \
	brw_vec4_vs_visitor.cpp \
	brw_wm_iz.cpp \
	gen6_gs_visitor.cpp

I965_FILES = \
	brw_binding_tables.c \
	brw_cc.c \
	brw_clear.c \
	brw_clip.c \
	brw_clip_line.c \
	brw_clip_point.c \
	brw_clip_state.c \
	brw_clip_tri.c \
	brw_clip_unfilled.c \
	brw_clip_util.c \
	brw_compute.c \
	brw_conditional_render.c \
	brw_context.c \
	brw_cs.c \
	brw_curbe.c \
	brw_draw.c \
	brw_draw_upload.c \
	brw_ff_gs.c \
	brw_ff_gs_emit.c \
	brw_gs.c \
	brw_gs_state.c \
	brw_gs_surface_state.c \
	brw_meta_fast_clear.c \
	brw_meta_stencil_blit.c \
	brw_meta_updownsample.c \
	brw_meta_util.c \
	brw_misc_state.c \
	brw_object_purgeable.c \
	brw_performance_monitor.c \
	brw_pipe_control.c \
	brw_program.c \
	brw_primitive_restart.c \
	brw_queryobj.c \
	brw_reset.c \
	brw_sampler_state.c \
	brw_sf.c \
	brw_sf_emit.c \
	brw_sf_state.c \
	brw_state_batch.c \
	brw_state_cache.c \
	brw_state_dump.c \
	brw_state_upload.c \
	brw_tcs.c \
	brw_tcs_surface_state.c \
	brw_tes.c \
	brw_tes_surface_state.c \
	brw_tex.c \
	brw_tex_layout.c \
	brw_urb.c \
	brw_vs.c \
	brw_vs_state.c \
	brw_vs_surface_state.c \
	brw_wm.c \
	brw_wm_state.c \
	brw_wm_surface_state.c \
	gen6_cc.c \
	gen6_clip_state.c \
	gen6_constant_state.c \
	gen6_depth_state.c \
	gen6_depthstencil.c \
	gen6_gs_state.c \
	gen6_multisample_state.c \
	gen6_queryobj.c \
	gen6_sampler_state.c \
	gen6_scissor_state.c \
	gen6_sf_state.c \
	gen6_sol.c \
	gen6_surface_state.c \
	gen6_urb.c \
	gen6_viewport_state.c \
	gen6_vs_state.c \
	gen6_wm_state.c \
	gen7_cs_state.c \
	gen7_ds_state.c \
	gen7_gs_state.c \
	gen7_hs_state.c \
	gen7_l3_state.c \
	gen7_misc_state.c \
	gen7_sf_state.c \
	gen7_sol_state.c \
	gen7_te_state.c \
	gen7_urb.c \
	gen7_viewport_state.c \
	gen7_vs_state.c \
	gen7_wm_state.c \
	gen7_wm_surface_state.c \
	gen8_blend_state.c \
	gen8_depth_state.c \
	gen8_disable.c \
	gen8_draw_upload.c \
	gen8_ds_state.c \
	gen8_gs_state.c \
	gen8_hs_state.c \
	gen8_misc_state.c \
	gen8_multisample_state.c \
	gen8_ps_state.c \
	gen8_sf_state.c \
	gen8_sol_state.c \
	gen8_surface_state.c \
	gen8_viewport_state.c \
	gen8_vs_state.c \
	gen8_wm_depth_stencil.c \
	intel_batchbuffer.c \
	intel_blit.c \
	intel_buffer_objects.c \
	intel_buffers.c \
	intel_copy_image.c \
	intel_extensions.c \
	intel_fbo.c \
	intel_mipmap_tree.c \
	intel_pixel_bitmap.c \
	intel_pixel.c \
	intel_pixel_copy.c \
	intel_pixel_draw.c \
	intel_pixel_read.c \
	intel_screen.c \
	intel_state.c \
	intel_syncobj.c \
	intel_tex.c \
	intel_tex_copy.c \
	intel_tex_image.c \
	intel_tex_subimage.c \
	intel_tex_validate.c \
	intel_tiled_memcpy.c \
	intel_upload.c


I965_FILES_CXX = \
	brw_blorp_blit.cpp \
	brw_blorp_blit_eu.cpp \
	brw_blorp.cpp \
	brw_cubemap_normalize.cpp \
	brw_fs_channel_expressions.cpp \
	brw_fs_vector_splitting.cpp \
	brw_link.cpp \
	brw_lower_texture_gradients.cpp \
	brw_lower_unnormalized_offset.cpp \
	gen6_blorp.cpp \
	gen7_blorp.cpp

INC_DIR += $(MESA_SRC_DIR)/drivers/dri/common

SRC_C  = $(addprefix drivers/dri/i965/,$(I965_COMPILER_FILES) $(I965_FILES))
SRC_CC = $(addprefix drivers/dri/i965/,$(I965_COMPILER_FILES_CXX) $(I965_FILES_CXX))

SRC_C += bo_map.c

CC_OPT      = -DHAVE_PTHREAD -DFFS_DEFINED=1
CC_C_OPT    = -std=c99
CC_CXX_OPT += -D__STDC_LIMIT_MACROS

#
# special flags
#
CC_OPT_drivers/dri/i965/brw_fs_combine_constants = -D__BSD_VISIBLE

vpath %.c   $(REP_DIR)/src/lib/mesa/i965
vpath %.c   $(MESA_SRC_DIR)
vpath %.cpp $(MESA_SRC_DIR)

CC_CXX_WARN_STRICT =
