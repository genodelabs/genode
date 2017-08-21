SHARED_LIB = yes
LIBS       = libc stdcxx expat glapi pthread mesa_api

include $(REP_DIR)/lib/mk/mesa-common.inc

MAIN_ES_FILES = \
	main/es1_conversion.c

MAIN_FILES = \
	main/accum.c \
	main/api_arrayelt.c \
	main/api_exec.c \
	main/api_loopback.c \
	main/api_validate.c \
	main/arbprogram.c \
	main/arrayobj.c \
	main/atifragshader.c \
	main/attrib.c \
	main/blend.c \
	main/blit.c \
	main/bufferobj.c \
	main/buffers.c \
	main/clear.c \
	main/clip.c \
	main/colortab.c \
	main/compute.c \
	main/condrender.c \
	main/context.c \
	main/convolve.c \
	main/copyimage.c \
	main/cpuinfo.c \
	main/debug.c \
	main/debug_output.c \
	main/depth.c \
	main/dlist.c \
	main/drawpix.c \
	main/drawtex.c \
	main/enable.c \
	main/enums.c \
	main/errors.c \
	main/eval.c \
	main/execmem.c \
	main/extensions.c \
	main/fbobject.c \
	main/feedback.c \
	main/ffvertex_prog.c \
	main/fog.c \
	main/format_pack.c \
	main/format_unpack.c \
	main/formatquery.c \
	main/formats.c \
	main/format_utils.c \
	main/framebuffer.c \
	main/get.c \
	main/genmipmap.c \
	main/getstring.c \
	main/glformats.c \
	main/hash.c \
	main/hint.c \
	main/histogram.c \
	main/image.c \
	main/imports.c \
	main/light.c \
	main/lines.c \
	main/matrix.c \
	main/mipmap.c \
	main/mm.c \
	main/multisample.c \
	main/objectlabel.c \
	main/objectpurge.c \
	main/pack.c \
	main/pbo.c \
	main/performance_monitor.c \
	main/pipelineobj.c \
	main/pixel.c \
	main/pixelstore.c \
	main/pixeltransfer.c \
	main/points.c \
	main/polygon.c \
	main/program_resource.c \
	main/querymatrix.c \
	main/queryobj.c \
	main/rastpos.c \
	main/readpix.c \
	main/remap.c \
	main/renderbuffer.c \
	main/samplerobj.c \
	main/scissor.c \
	main/shaderapi.c \
	main/shaderimage.c \
	main/shaderobj.c \
	main/shared.c \
	main/state.c \
	main/stencil.c \
	main/syncobj.c \
	main/texcompress.c \
	main/texcompress_bptc.c \
	main/texcompress_cpal.c \
	main/texcompress_etc.c \
	main/texcompress_fxt1.c \
	main/texcompress_rgtc.c \
	main/texcompress_s3tc.c \
	main/texenv.c \
	main/texformat.c \
	main/texgen.c \
	main/texgetimage.c \
	main/teximage.c \
	main/texobj.c \
	main/texparam.c \
	main/texstate.c \
	main/texstorage.c \
	main/texstore.c \
	main/textureview.c \
	main/texturebarrier.c \
	main/transformfeedback.c \
	main/uniforms.c \
	main/varray.c \
	main/vdpau.c \
	main/version.c \
	main/viewport.c \
	main/vtxfmt.c

MAIN_FILES_CC = \
	main/ff_fragment_shader.cpp \
	main/shader_query.cpp \
	main/uniform_query.cpp \

MATH_FILES = \
	math/m_debug_clip.c \
	math/m_debug_norm.c \
	math/m_debug_xform.c \
	math/m_eval.c \
	math/m_matrix.c \
	math/m_translate.c \
	math/m_vector.c \

MATH_XFORM_FILES = \
	math/m_xform.c \

VBO_FILES = \
	vbo/vbo_context.c \
	vbo/vbo_exec_api.c \
	vbo/vbo_exec_array.c \
	vbo/vbo_exec.c \
	vbo/vbo_exec_draw.c \
	vbo/vbo_exec_eval.c \
	vbo/vbo_minmax_index.c \
	vbo/vbo_noop.c \
	vbo/vbo_primitive_restart.c \
	vbo/vbo_rebase.c \
	vbo/vbo_save_api.c \
	vbo/vbo_save.c \
	vbo/vbo_save_draw.c \
	vbo/vbo_save_loopback.c \
	vbo/vbo_split.c \
	vbo/vbo_split_copy.c \
	vbo/vbo_split_inplace.c

TNL_FILES = \
	tnl/t_context.c \
	tnl/t_draw.c \
	tnl/t_pipeline.c \
	tnl/t_vb_fog.c \
	tnl/t_vb_light.c \
	tnl/t_vb_normals.c \
	tnl/t_vb_points.c \
	tnl/t_vb_program.c \
	tnl/t_vb_render.c \
	tnl/t_vb_texgen.c \
	tnl/t_vb_texmat.c \
	tnl/t_vb_vertex.c \
	tnl/t_vertex.c \
	tnl/t_vertex_generic.c \
	tnl/t_vertex_sse.c \
	tnl/t_vp_build.c \

SWRAST_FILES = \
	swrast/s_aaline.c \
	swrast/s_aatriangle.c \
	swrast/s_alpha.c \
	swrast/s_atifragshader.c \
	swrast/s_bitmap.c \
	swrast/s_blend.c \
	swrast/s_blit.c \
	swrast/s_clear.c \
	swrast/s_context.c \
	swrast/s_copypix.c \
	swrast/s_depth.c \
	swrast/s_drawpix.c \
	swrast/s_feedback.c \
	swrast/s_fog.c \
	swrast/s_fragprog.c \
	swrast/s_lines.c \
	swrast/s_logic.c \
	swrast/s_masking.c \
	swrast/s_points.c \
	swrast/s_renderbuffer.c \
	swrast/s_span.c \
	swrast/s_stencil.c \
	swrast/s_texcombine.c \
	swrast/s_texfetch.c \
	swrast/s_texfilter.c \
	swrast/s_texrender.c \
	swrast/s_texture.c \
	swrast/s_triangle.c \
	swrast/s_zoom.c \

SWRAST_SETUP_FILES = \
	swrast_setup/ss_context.c \
	swrast_setup/ss_triangle.c \

COMMON_DRIVER_FILES = \
	drivers/common/driverfuncs.c \
	drivers/common/meta_blit.c \
	drivers/common/meta_copy_image.c \
	drivers/common/meta_generate_mipmap.c \
	drivers/common/meta_tex_subimage.c \
	drivers/common/meta.c \

ASM_C_FILES += \
	x86/common_x86.c \
	x86/x86_xform.c \
	x86/sse.c \
	x86/rtasm/x86sse.c \
	sparc/sparc.c \

PROGRAM = \
	program/arbprogparse.c \
	program/lex.yy.c \
	program/prog_cache.c \
	program/prog_execute.c \
	program/prog_hash_table.c \
	program/prog_instruction.c \
	program/prog_noise.c \
	program/prog_opt_constant_fold.c \
	program/prog_optimize.c \
	program/prog_parameter.c \
	program/prog_parameter_layout.c \
	program/prog_print.c \
	program/prog_statevars.c \
	program/prog_to_nir.c \
	program/program.c \
	program/programopt.c \
	program/program_parse.tab.c \
	program/program_parse_extra.c \
	program/symbol_table.c

PROGRAM_CXX = \
	program/ir_to_mesa.cpp \
	program/string_to_uint_map.cpp

MAPI_FILES = \
	mapi/entry.c

UTIL = \
	util/debug.c \
	util/format_srgb.c \
	util/half_float.c \
	util/hash_table.c \
	util/ralloc.c \
	util/register_allocate.c \
	util/rgtc.c \
	util/set.c \
	util/strtod.c

GLSL = \
	glsl/blob.c \
	glsl/glcpp/glcpp-lex.c \
	glsl/glcpp/glcpp-parse.c \
	glsl/glcpp/pp.c

GLSL_CXX = \
	glsl/ast_array_index.cpp \
	glsl/ast_expr.cpp \
	glsl/ast_function.cpp \
	glsl/ast_to_hir.cpp \
	glsl/ast_type.cpp \
	glsl/builtin_functions.cpp \
	glsl/builtin_types.cpp \
	glsl/builtin_variables.cpp \
	glsl/glsl_lexer.cpp \
	glsl/glsl_parser.cpp \
	glsl/glsl_parser_extras.cpp \
	glsl/glsl_symbol_table.cpp \
	glsl/hir_field_selection.cpp \
	glsl/ir_basic_block.cpp \
	glsl/ir_builder.cpp \
	glsl/ir_clone.cpp \
	glsl/ir_constant_expression.cpp \
	glsl/ir.cpp \
	glsl/ir_equals.cpp \
	glsl/ir_expression_flattening.cpp \
	glsl/ir_function_can_inline.cpp \
	glsl/ir_function_detect_recursion.cpp \
	glsl/ir_function.cpp \
	glsl/ir_hierarchical_visitor.cpp \
	glsl/ir_hv_accept.cpp \
	glsl/ir_import_prototypes.cpp \
	glsl/ir_print_visitor.cpp \
	glsl/ir_reader.cpp \
	glsl/ir_rvalue_visitor.cpp \
	glsl/ir_set_program_inouts.cpp \
	glsl/ir_validate.cpp \
	glsl/ir_variable_refcount.cpp \
	glsl/linker.cpp \
	glsl/link_atomics.cpp \
	glsl/link_functions.cpp \
	glsl/link_interface_blocks.cpp \
	glsl/link_uniforms.cpp \
	glsl/link_uniform_initializers.cpp \
	glsl/link_uniform_block_active_visitor.cpp \
	glsl/link_uniform_blocks.cpp \
	glsl/link_varyings.cpp \
	glsl/loop_analysis.cpp \
	glsl/loop_controls.cpp \
	glsl/loop_unroll.cpp \
	glsl/lower_buffer_access.cpp \
	glsl/lower_clip_distance.cpp \
	glsl/lower_const_arrays_to_uniforms.cpp \
	glsl/lower_discard.cpp \
	glsl/lower_discard_flow.cpp \
	glsl/lower_if_to_cond_assign.cpp \
	glsl/lower_instructions.cpp \
	glsl/lower_jumps.cpp \
	glsl/lower_mat_op_to_vec.cpp \
	glsl/lower_noise.cpp \
	glsl/lower_offset_array.cpp \
	glsl/lower_packed_varyings.cpp \
	glsl/lower_named_interface_blocks.cpp \
	glsl/lower_packing_builtins.cpp \
	glsl/lower_subroutine.cpp \
	glsl/lower_tess_level.cpp \
	glsl/lower_texture_projection.cpp \
	glsl/lower_variable_index_to_cond_assign.cpp \
	glsl/lower_vec_index_to_cond_assign.cpp \
	glsl/lower_vec_index_to_swizzle.cpp \
	glsl/lower_vector.cpp \
	glsl/lower_vector_derefs.cpp \
	glsl/lower_vector_insert.cpp \
	glsl/lower_vertex_id.cpp \
	glsl/lower_output_reads.cpp \
	glsl/lower_shared_reference.cpp \
	glsl/lower_ubo_reference.cpp \
	glsl/opt_algebraic.cpp \
	glsl/opt_array_splitting.cpp \
	glsl/opt_conditional_discard.cpp \
	glsl/opt_constant_folding.cpp \
	glsl/opt_constant_propagation.cpp \
	glsl/opt_constant_variable.cpp \
	glsl/opt_copy_propagation.cpp \
	glsl/opt_copy_propagation_elements.cpp \
	glsl/opt_dead_builtin_variables.cpp \
	glsl/opt_dead_builtin_varyings.cpp \
	glsl/opt_dead_code.cpp \
	glsl/opt_dead_code_local.cpp \
	glsl/opt_dead_functions.cpp \
	glsl/opt_flatten_nested_if_blocks.cpp \
	glsl/opt_flip_matrices.cpp \
	glsl/opt_function_inlining.cpp \
	glsl/opt_if_simplification.cpp \
	glsl/opt_minmax.cpp \
	glsl/opt_noop_swizzle.cpp \
	glsl/opt_rebalance_tree.cpp \
	glsl/opt_redundant_jumps.cpp \
	glsl/opt_structure_splitting.cpp \
	glsl/opt_swizzle_swizzle.cpp \
	glsl/opt_tree_grafting.cpp \
	glsl/opt_vectorize.cpp \
	glsl/s_expression.cpp

LIBCOMPILER_CXX = \
	glsl_types.cpp \
	nir_types.cpp

LIBCOMPILER = \
	shader_enums.c

DRI = \
	drivers/dri/common/dri_util.c \
	drivers/dri/common/megadriver_stub.c \
	drivers/dri/common/utils.c \
	drivers/dri/common/xmlconfig.c

NIR = \
	compiler/nir/nir.c \
	compiler/nir/nir_clone.c \
	compiler/nir/nir_constant_expressions.c \
	compiler/nir/nir_control_flow.c \
	compiler/nir/nir_dominance.c \
	compiler/nir/nir_from_ssa.c \
	compiler/nir/nir_instr_set.c \
	compiler/nir/nir_intrinsics.c \
	compiler/nir/nir_gs_count_vertices.c \
	compiler/nir/nir_liveness.c \
	compiler/nir/nir_lower_alu_to_scalar.c \
	compiler/nir/nir_lower_atomics.c \
	compiler/nir/nir_lower_global_vars_to_local.c \
	compiler/nir/nir_lower_gs_intrinsics.c \
	compiler/nir/nir_lower_io.c \
	compiler/nir/nir_lower_load_const_to_scalar.c \
	compiler/nir/nir_lower_locals_to_regs.c \
	compiler/nir/nir_lower_outputs_to_temporaries.c \
	compiler/nir/nir_lower_phis_to_scalar.c \
	compiler/nir/nir_lower_samplers.c \
	compiler/nir/nir_lower_system_values.c \
	compiler/nir/nir_lower_to_source_mods.c \
	compiler/nir/nir_lower_tex.c \
	compiler/nir/nir_lower_vars_to_ssa.c \
	compiler/nir/nir_lower_var_copies.c \
	compiler/nir/nir_lower_vec_to_movs.c \
	compiler/nir/nir_metadata.c \
	compiler/nir/nir_move_vec_src_uses_to_dest.c \
	compiler/nir/nir_normalize_cubemap_coords.c \
	compiler/nir/nir_opcodes.c \
	compiler/nir/nir_opt_algebraic.c \
	compiler/nir/nir_opt_constant_folding.c \
	compiler/nir/nir_opt_copy_propagate.c \
	compiler/nir/nir_opt_cse.c \
	compiler/nir/nir_opt_dce.c \
	compiler/nir/nir_opt_dead_cf.c \
	compiler/nir/nir_opt_peephole_select.c \
	compiler/nir/nir_opt_remove_phis.c \
	compiler/nir/nir_opt_undef.c \
	compiler/nir/nir_print.c \
	compiler/nir/nir_remove_dead_variables.c \
	compiler/nir/nir_search.c \
	compiler/nir/nir_split_var_copies.c \
	compiler/nir/nir_sweep.c \
	compiler/nir/nir_to_ssa.c \
	compiler/nir/nir_worklist.c

NIR_CXX = \
	nir/glsl_to_nir.cpp

SRC_C = \
	$(MAIN_ES_FILES) \
	$(MAIN_FILES) \
	$(MATH_FILES) \
	$(MAPI_FILES) \
	$(MATH_XFORM_FILES) \
	$(VBO_FILES) \
	$(TNL_FILES) \
	$(SWRAST_FILES) \
	$(SWRAST_SETUP_FILES) \
	$(COMMON_DRIVER_FILES) \
	$(ASM_C_FILES) \
	$(PROGRAM) \
	$(UTIL) \
	$(GLSL) \
	$(LIBCOMPILER) \
	$(DRI) \
	$(NIR)

SRC_CC = $(MAIN_FILES_CC) $(GLSL_CXX) $(LIBCOMPILER_CXX) $(PROGRAM_CXX) $(NIR_CXX)

CC_OPT += -DPACKAGE_VERSION=\"11.2.2\" \
          -DPACKAGE_BUGREPORT=\"https://bugs.freedesktop.org/enter_bug.cgi\?product=Mesa\" \
          -DMAPI_ABI_HEADER=\"glapi/glapi_mapi_tmp.h\" \
          -DMAPI_MODE_BRIDGE

#XXX: for debugging
CC_OPT += -gdwarf-2

CC_C_OPT   += -std=c99
CC_CXX_OPT += -D__STDC_CONSTANT_MACROS -D__STDC_LIMIT_MACROS

#
# special flags
#
CC_OPT_glsl/linker                        = -D__BSD_VISIBLE
CC_OPT_compiler/nir/nir_from_ssa          = -D__BSD_VISIBLE -DFFS_DEFINED=1
CC_OPT_compiler/nir/nir_instr_set         = -D__BSD_VISIBLE -DFFS_DEFINED=1
CC_OPT_compiler/nir/nir_liveness          = -D__BSD_VISIBLE -DFFS_DEFINED=1
CC_OPT_compiler/nir/nir_lower_vars_to_ssa = -D__BSD_VISIBLE -DFFS_DEFINED=1

vpath %.c    $(MESA_SRC_DIR)
vpath %.cpp  $(MESA_SRC_DIR)
vpath %.c    $(MESA_PORT_DIR)/src
vpath %.cpp  $(MESA_PORT_DIR)/src/compiler
vpath %.c    $(MESA_PORT_DIR)/src/compiler

# vi: set ft=make :
