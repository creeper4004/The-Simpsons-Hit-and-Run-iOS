#ifndef SHAR_MGL_SHADER_INTERFACE_H
#define SHAR_MGL_SHADER_INTERFACE_H

#include <glslang/Include/glslang_c_interface.h>

// AUTO_MAP_LOCATIONS only enables mapping; it does not run it. Use the GLSL
// resolver/mapper so linked stages agree by variable name, including when their
// declaration orders differ. Call after link and before SPIR-V generation.
static inline int sharMapShaderInterface(glslang_program_t* program, glslang_stage_t stage)
{
    glslang_resolver_t* resolver = glslang_glsl_resolver_create(program, stage);
    glslang_mapper_t* mapper = glslang_glsl_mapper_create();
    int result = glslang_program_map_io_with_resolver_and_mapper(program, resolver, mapper);
    glslang_glsl_mapper_delete(mapper);
    glslang_glsl_resolver_delete(resolver);
    return result;
}

#endif
