/*
 * glm_params_ios.c — iOS replacement for MGL/src/glm_params.c
 *
 * Master's getMacOSDefaults() dlopens the macOS desktop OpenGL (CGL) to read
 * guestimate caps/state. iOS has no desktop GL, so hardcode the same values a
 * fresh 4.x-core CGL context would report on Apple-GPU-class hardware, plus
 * the MGL iOS bring-up caps (max_texture_size=16384, max_3d=2048,
 * max_cube_map=16384, max_vertex_attribs=16, max_color_attachments=8).
 * Compiled INSTEAD of glm_params.c (its only other content is this function).
 */

#include "glcorearb.h"
#include "glm_context.h"

void getMacOSDefaults(GLMContext glm_ctx)
{
    GLMParams *v = &glm_ctx->state.var;

    // ---- GL defaults for a fresh context (state the engine may read before
    // it configures anything) ----
    v->point_size = 1.0f;
    v->point_size_range = 255;
    v->point_size_granularity = 1;
    v->line_width = 1.0f;
    v->line_width_range = 10;
    v->line_width_granularity = 1;
    v->polygon_mode = GL_FILL;
    v->cull_face_mode = GL_BACK;
    v->front_face = GL_CCW;
    v->depth_range[0] = 0.0;      v->depth_range[1] = 1.0;
    v->depth_writemask = GL_TRUE;
    v->depth_clear_value = 1.0;
    v->depth_func = GL_LESS;
    v->stencil_clear_value = 0;
    v->stencil_func = GL_ALWAYS;
    v->stencil_value_mask = 0xFFFFFFFF;
    v->stencil_fail = GL_KEEP;
    v->stencil_pass_depth_fail = GL_KEEP;
    v->stencil_pass_depth_pass = GL_KEEP;
    v->stencil_ref = 0;
    v->stencil_writemask = 0xFFFFFFFF;
    v->scissor_box[0] = 0; v->scissor_box[1] = 0;
    v->scissor_box[2] = 0; v->scissor_box[3] = 0;
    v->blend_dst_rgb[0] = GL_ZERO;
    v->blend_src_rgb[0] = GL_ONE;
    v->blend_dst_alpha[0] = GL_ZERO;
    v->blend_src_alpha[0] = GL_ONE;
    v->blend_equation_rgb[0] = GL_FUNC_ADD;
    v->blend_equation_alpha[0] = GL_FUNC_ADD;
    v->blend_color[0] = 0.0f; v->blend_color[1] = 0.0f;
    v->blend_color[2] = 0.0f; v->blend_color[3] = 0.0f;
    v->logic_op_mode = GL_COPY;
    v->color_writemask[0][0] = GL_TRUE; v->color_writemask[0][1] = GL_TRUE;
    v->color_writemask[0][2] = GL_TRUE; v->color_writemask[0][3] = GL_TRUE;
    v->sample_coverage_value = 1.0f;
    v->sample_coverage_invert = GL_FALSE;
    v->polygon_offset_units = 0.0f;
    v->polygon_offset_factor = 0.0f;
    v->current_program = 0;
    v->array_buffer_binding = 0;
    v->element_array_buffer_binding = 0;
    v->texture_binding_1d = 0;
    v->texture_binding_2d = 0;
    v->texture_binding_3d = 0;
    v->texture_binding_cube_map = 0;
    v->texture_binding_1d_array = 0;
    v->texture_binding_2d_array = 0;
    v->texture_binding_buffer = 0;
    v->texture_binding_rectangle = 0;
    v->draw_framebuffer_binding = 0;
    v->read_framebuffer_binding = 0;
    v->renderbuffer_binding = 0;
    v->vertex_array_binding = 0;
    v->pixel_pack_buffer_binding = 0;
    v->pixel_unpack_buffer_binding = 0;
    v->uniform_buffer_binding = 0;
    v->uniform_buffer_start = 0;
    v->uniform_buffer_size = 0;
    v->sampler_binding = 0;
    v->stencil_back_func = GL_ALWAYS;
    v->stencil_back_ref = 0;
    v->stencil_back_value_mask = 0xFFFFFFFF;
    v->stencil_back_writemask = 0xFFFFFFFF;
    v->stencil_back_fail = GL_KEEP;
    v->stencil_back_pass_depth_fail = GL_KEEP;
    v->stencil_back_pass_depth_pass = GL_KEEP;
    v->primitive_restart_index = 0;
    v->context_flags = 0;
    v->major_version = 4;
    v->minor_version = 6;
    v->num_extensions = 0;
    v->num_compressed_texture_formats = 0;

    // ---- caps (Apple-GPU-class / MGL iOS bring-up values) ----
    v->max_texture_size = 16384;
    v->max_3d_texture_size = 2048;
    v->max_cube_map_texture_size = 16384;
    v->max_rectangle_texture_size = 16384;
    v->max_renderbuffer_size = 16384;
    v->max_viewport_dims = 16384;
    v->subpixel_bits = 4;
    v->max_texture_image_units = 16;
    v->max_vertex_texture_image_units = 16;
    v->max_combined_texture_image_units = 32;
    v->max_texture_lod_bias = 16.0f;
    v->max_array_texture_layers = 2048;
    v->max_clip_distances = 8;
    v->max_draw_buffers = 8;
    v->max_elements_vertices = 1048576;
    v->max_elements_indices = 1048576;
    v->smooth_point_size_range = 255;
    v->smooth_point_size_granularity = 1.0f;
    v->smooth_line_width_range = 10;
    v->smooth_line_width_granularity = 1.0f;
    v->aliased_line_width_range = 10;
    v->max_texture_buffer_size = 65536;
    v->min_program_texel_offset = -8;
    v->max_program_texel_offset = 7;
    v->max_fragment_uniform_components = 1024;
    v->max_vertex_uniform_components = 1024;
    v->max_varying_floats = 64;
    v->max_varying_components = 64;
    v->max_vertex_uniform_blocks = 12;
    v->max_fragment_uniform_blocks = 12;
    v->max_geometry_uniform_blocks = 0;
    v->max_combined_uniform_blocks = 24;
    v->max_uniform_buffer_bindings = 36;
    v->max_uniform_block_size = 16384;
    v->max_combined_vertex_uniform_components = 4096;
    v->max_combined_fragment_uniform_components = 4096;
    v->max_combined_geometry_uniform_components = 0;
    v->uniform_buffer_offset_alignment = 256;
    v->max_geometry_texture_image_units = 0;
    v->max_geometry_uniform_components = 0;
    v->max_vertex_output_components = 64;
    v->max_geometry_input_components = 0;
    v->max_geometry_output_components = 0;
    v->max_tess_control_uniform_blocks = 0;
    v->max_tess_evaluation_uniform_blocks = 0;
    v->max_vertex_uniform_vectors = 256;
    v->max_varying_vectors = 16;
    v->max_fragment_uniform_vectors = 256;
    v->shader_compiler = 1;
    v->num_shader_binary_formats = 0;
    v->program_pipeline_binding = 0;

    // state-level (outside var)
    glm_ctx->state.max_vertex_attribs = 16;
    glm_ctx->state.max_color_attachments = 8;
}
