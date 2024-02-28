LIBS = libc

include $(REP_DIR)/lib/mk/mesa-common.inc

INC_DIR += $(MESA_SRC_DIR)/src/gallium/auxiliary \
           $(MESA_SRC_DIR)/src/compiler/nir \
           $(MESA_GEN_DIR)/src/compiler/nir \

SRC_C =  gallium/drivers/softpipe/sp_buffer.c \
         gallium/drivers/softpipe/sp_clear.c \
         gallium/drivers/softpipe/sp_compute.c \
         gallium/drivers/softpipe/sp_context.c \
         gallium/drivers/softpipe/sp_draw_arrays.c \
         gallium/drivers/softpipe/sp_fence.c \
         gallium/drivers/softpipe/sp_flush.c \
         gallium/drivers/softpipe/sp_fs_exec.c \
         gallium/drivers/softpipe/sp_image.c \
         gallium/drivers/softpipe/sp_prim_vbuf.c \
         gallium/drivers/softpipe/sp_quad_blend.c \
         gallium/drivers/softpipe/sp_quad_depth_test.c \
         gallium/drivers/softpipe/sp_quad_fs.c \
         gallium/drivers/softpipe/sp_quad_pipe.c \
         gallium/drivers/softpipe/sp_query.c \
         gallium/drivers/softpipe/sp_screen.c \
         gallium/drivers/softpipe/sp_setup.c \
         gallium/drivers/softpipe/sp_state_blend.c \
         gallium/drivers/softpipe/sp_state_clip.c \
         gallium/drivers/softpipe/sp_state_derived.c \
         gallium/drivers/softpipe/sp_state_image.c \
         gallium/drivers/softpipe/sp_state_rasterizer.c \
         gallium/drivers/softpipe/sp_state_sampler.c \
         gallium/drivers/softpipe/sp_state_shader.c \
         gallium/drivers/softpipe/sp_state_so.c \
         gallium/drivers/softpipe/sp_state_surface.c \
         gallium/drivers/softpipe/sp_state_vertex.c \
         gallium/drivers/softpipe/sp_surface.c \
         gallium/drivers/softpipe/sp_tex_sample.c \
         gallium/drivers/softpipe/sp_tex_tile_cache.c \
         gallium/drivers/softpipe/sp_texture.c \
         gallium/drivers/softpipe/sp_tile_cache.c \

vpath %.c $(MESA_SRC_DIR)/src

