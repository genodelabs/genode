LIBS = libc libdrm stdcxx \
       iris_gen80 iris_gen90 iris_gen110 iris_gen120 iris_gen125 iris_gen200\
       isl_gen80  isl_gen90  isl_gen110  isl_gen120  isl_gen125  isl_gen200

LIBS += expat zlib

CC_CXX_OPT_STD = -std=c++17

include $(REP_DIR)/lib/mk/mesa-common.inc

CC_CXX_WARN_STRICT =

CC_OPT   += -DGALLIUM_IRIS -DHAVE_LIBDRM
# We rename 'ioctl' calls to 'genode_ioctl' calls (drm lib)
CC_C_OPT += -Dioctl=genode_ioctl
CC_C_OPT += -DUSE_SSE41 -msse4

CC_OPT += -Wno-unused-function

INC_DIR += $(MESA_GEN_DIR)/src/compiler \
           $(MESA_GEN_DIR)/src/compiler/nir \
           $(MESA_GEN_DIR)/src/intel \
           $(MESA_GEN_DIR)/src/intel/dev \
           $(MESA_GEN_DIR)/src/intel/ds \

INC_DIR += $(MESA_SRC_DIR)/src/compiler \
           $(MESA_SRC_DIR)/src/compiler/nir \
           $(MESA_SRC_DIR)/src/gallium/auxiliary \
           $(MESA_SRC_DIR)/src/gallium/drivers \
           $(MESA_SRC_DIR)/src/gallium/drivers/iris \
           $(MESA_SRC_DIR)/src/intel \
           $(MESA_SRC_DIR)/src/intel/common \
           $(MESA_SRC_DIR)/src/intel/dev \
           $(MESA_SRC_DIR)/src/intel/ds \
           $(MESA_SRC_DIR)/src/mapi \
           $(MESA_SRC_DIR)/src/mesa \
           $(MESA_SRC_DIR)/src/mesa/main \

SRC_CC += intel/compiler/brw_cfg.cpp \
          intel/compiler/brw_dead_control_flow.cpp \
          intel/compiler/brw_fs.cpp \
          intel/compiler/brw_fs_bank_conflicts.cpp \
          intel/compiler/brw_fs_cmod_propagation.cpp \
          intel/compiler/brw_fs_combine_constants.cpp \
          intel/compiler/brw_fs_copy_propagation.cpp \
          intel/compiler/brw_fs_cse.cpp \
          intel/compiler/brw_fs_dead_code_eliminate.cpp \
          intel/compiler/brw_fs_generator.cpp \
          intel/compiler/brw_fs_live_variables.cpp \
          intel/compiler/brw_fs_lower_dpas.cpp \
          intel/compiler/brw_fs_lower_pack.cpp \
          intel/compiler/brw_fs_lower_regioning.cpp \
          intel/compiler/brw_fs_nir.cpp \
          intel/compiler/brw_fs_reg_allocate.cpp \
          intel/compiler/brw_fs_register_coalesce.cpp \
          intel/compiler/brw_fs_saturate_propagation.cpp \
          intel/compiler/brw_fs_scoreboard.cpp \
          intel/compiler/brw_fs_sel_peephole.cpp \
          intel/compiler/brw_fs_thread_payload.cpp \
          intel/compiler/brw_fs_validate.cpp \
          intel/compiler/brw_fs_visitor.cpp \
          intel/compiler/brw_ir_performance.cpp \
          intel/compiler/brw_lower_logical_sends.cpp \
          intel/compiler/brw_mesh.cpp \
          intel/compiler/brw_predicated_break.cpp \
          intel/compiler/brw_schedule_instructions.cpp \
          intel/compiler/brw_shader.cpp \
          intel/compiler/brw_simd_selection.cpp \
          intel/compiler/brw_vec4.cpp \
          intel/compiler/brw_vec4_cmod_propagation.cpp \
          intel/compiler/brw_vec4_copy_propagation.cpp \
          intel/compiler/brw_vec4_cse.cpp \
          intel/compiler/brw_vec4_dead_code_eliminate.cpp \
          intel/compiler/brw_vec4_generator.cpp \
          intel/compiler/brw_vec4_gs_nir.cpp \
          intel/compiler/brw_vec4_gs_visitor.cpp \
          intel/compiler/brw_vec4_live_variables.cpp \
          intel/compiler/brw_vec4_nir.cpp \
          intel/compiler/brw_vec4_reg_allocate.cpp \
          intel/compiler/brw_vec4_surface_builder.cpp \
          intel/compiler/brw_vec4_tcs.cpp \
          intel/compiler/brw_vec4_tes.cpp \
          intel/compiler/brw_vec4_visitor.cpp \
          intel/compiler/brw_vec4_vs_visitor.cpp \
          intel/compiler/gfx6_gs_visitor.cpp \
          intel/ds/intel_driver_ds.cc

SRC_C += gallium/drivers/iris/i915/iris_batch.c \
         gallium/drivers/iris/i915/iris_bufmgr.c \
         gallium/drivers/iris/i915/iris_kmd_backend.c \
         gallium/drivers/iris/iris_batch.c \
         gallium/drivers/iris/iris_binder.c \
         gallium/drivers/iris/iris_blit.c \
         gallium/drivers/iris/iris_border_color.c \
         gallium/drivers/iris/iris_bufmgr.c \
         gallium/drivers/iris/iris_clear.c \
         gallium/drivers/iris/iris_context.c \
         gallium/drivers/iris/iris_disk_cache.c \
         gallium/drivers/iris/iris_draw.c \
         gallium/drivers/iris/iris_fence.c \
         gallium/drivers/iris/iris_fine_fence.c \
         gallium/drivers/iris/iris_formats.c \
         gallium/drivers/iris/iris_kmd_backend.c \
         gallium/drivers/iris/iris_measure.c \
         gallium/drivers/iris/iris_monitor.c \
         gallium/drivers/iris/iris_perf.c \
         gallium/drivers/iris/iris_performance_query.c \
         gallium/drivers/iris/iris_pipe_control.c \
         gallium/drivers/iris/iris_program.c \
         gallium/drivers/iris/iris_program_cache.c \
         gallium/drivers/iris/iris_resolve.c \
         gallium/drivers/iris/iris_resource.c \
         gallium/drivers/iris/iris_screen.c \
         gallium/drivers/iris/iris_utrace.c \
         intel/blorp/blorp.c \
         intel/blorp/blorp_blit.c \
         intel/blorp/blorp_clear.c \
         intel/common/i915/intel_engine.c \
         intel/common/i915/intel_gem.c \
         intel/common/intel_aux_map.c \
         intel/common/intel_batch_decoder.c \
         intel/common/intel_decoder.c \
         intel/common/intel_disasm.c \
         intel/common/intel_engine.c \
         intel/common/intel_gem.c \
         intel/common/intel_l3_config.c \
         intel/common/intel_measure.c \
         intel/common/intel_mem.c \
         intel/common/intel_sample_positions.c \
         intel/common/intel_urb_config.c \
         intel/common/intel_uuid.c \
         intel/common/xe/intel_device_query.c \
         intel/common/xe/intel_engine.c \
         intel/common/xe/intel_gem.c \
         intel/compiler/brw_clip_line.c \
         intel/compiler/brw_clip_point.c \
         intel/compiler/brw_clip_tri.c \
         intel/compiler/brw_clip_unfilled.c \
         intel/compiler/brw_clip_util.c \
         intel/compiler/brw_compile_clip.c \
         intel/compiler/brw_compile_ff_gs.c \
         intel/compiler/brw_compile_sf.c \
         intel/compiler/brw_compiler.c \
         intel/compiler/brw_debug_recompile.c \
         intel/compiler/brw_disasm.c \
         intel/compiler/brw_disasm_info.c \
         intel/compiler/brw_eu.c \
         intel/compiler/brw_eu_compact.c \
         intel/compiler/brw_eu_emit.c \
         intel/compiler/brw_eu_util.c \
         intel/compiler/brw_eu_validate.c \
         intel/compiler/brw_interpolation_map.c \
         intel/compiler/brw_nir.c \
         intel/compiler/brw_nir_analyze_boolean_resolves.c \
         intel/compiler/brw_nir_analyze_ubo_ranges.c \
         intel/compiler/brw_nir_attribute_workarounds.c \
         intel/compiler/brw_nir_blockify_uniform_loads.c \
         intel/compiler/brw_nir_clamp_image_1d_2d_array_sizes.c \
         intel/compiler/brw_nir_clamp_per_vertex_loads.c \
         intel/compiler/brw_nir_lower_alpha_to_coverage.c \
         intel/compiler/brw_nir_lower_conversions.c \
         intel/compiler/brw_nir_lower_cooperative_matrix.c \
         intel/compiler/brw_nir_lower_cs_intrinsics.c \
         intel/compiler/brw_nir_lower_intersection_shader.c \
         intel/compiler/brw_nir_lower_non_uniform_barycentric_at_sample.c \
         intel/compiler/brw_nir_lower_non_uniform_resource_intel.c \
         intel/compiler/brw_nir_lower_ray_queries.c \
         intel/compiler/brw_nir_lower_rt_intrinsics.c \
         intel/compiler/brw_nir_lower_shader_calls.c \
         intel/compiler/brw_nir_lower_shading_rate_output.c \
         intel/compiler/brw_nir_lower_sparse.c \
         intel/compiler/brw_nir_lower_storage_image.c \
         intel/compiler/brw_nir_opt_peephole_ffma.c \
         intel/compiler/brw_nir_opt_peephole_imul32x16.c \
         intel/compiler/brw_nir_rt.c \
         intel/compiler/brw_nir_tcs_workarounds.c \
         intel/compiler/brw_packed_float.c \
         intel/compiler/brw_reg_type.c \
         intel/compiler/brw_vue_map.c \
         intel/dev/i915/intel_device_info.c \
         intel/dev/intel_debug.c \
         intel/dev/intel_device_info.c \
         intel/dev/intel_hwconfig.c \
         intel/dev/intel_kmd.c \
         intel/dev/xe/intel_device_info.c \
         intel/isl/isl.c \
         intel/isl/isl_aux_info.c \
         intel/isl/isl_drm.c \
         intel/isl/isl_format.c \
         intel/isl/isl_gfx12.c \
         intel/isl/isl_gfx4.c \
         intel/isl/isl_gfx6.c \
         intel/isl/isl_gfx7.c \
         intel/isl/isl_gfx8.c \
         intel/isl/isl_gfx9.c \
         intel/isl/isl_query.c \
         intel/isl/isl_storage_image.c \
         intel/isl/isl_tiled_memcpy.c \
         intel/isl/isl_tiled_memcpy_normal.c \
         intel/isl/isl_tiled_memcpy_sse41.c \
         intel/perf/intel_perf.c \
         intel/perf/intel_perf_mdapi.c \
         intel/perf/intel_perf_query.c \

#
# generated
#
SRC_C += intel/dev/intel_wa.c \
         intel/ds/intel_tracepoints.c \
         intel/isl/isl_format_layout.c \
         intel/perf/intel_perf_metrics.c \

vpath %.c   $(MESA_GEN_DIR)/src
vpath %.c   $(MESA_SRC_DIR)/src
vpath %.cc  $(MESA_SRC_DIR)/src
vpath %.cpp $(MESA_SRC_DIR)/src
