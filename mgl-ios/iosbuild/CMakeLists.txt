# MGL core static lib for iOS simulator (arm64) — merged with SPIRV-Cross.
# glslang/SPIRV/SPIRV-Tools/default-resource-limits are separate (engine links them).
cmake_minimum_required(VERSION 3.16)
project(mgl_ios C CXX OBJC)

set(MGL_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
if(NOT DEFINED MGL_STAGE)
  set(MGL_STAGE "/tmp/mgl-staged")
endif()

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(MGL_C_SOURCES
  ${MGL_ROOT}/MGL/src/buffers.c
  ${MGL_ROOT}/MGL/src/compute.c
  ${MGL_ROOT}/MGL/src/draw_buffers.c
  ${MGL_ROOT}/MGL/src/error.c
  ${MGL_ROOT}/MGL/src/fence.c
  ${MGL_ROOT}/MGL/src/framebuffers.c
  ${MGL_ROOT}/MGL/src/get.c
  ${MGL_ROOT}/MGL/src/gl_core.c
  ${MGL_ROOT}/MGL/src/glm_context.c
  ${MGL_ROOT}/MGL/src/glm_dispatch.c
  ${MGL_ROOT}/MGL/src/mgl_funcs_to_be_implemented.c
  ${MGL_ROOT}/MGL/src/mgl_toolchain.c
  ${MGL_ROOT}/MGL/src/non_core_unimplemented.c
  ${MGL_ROOT}/MGL/src/pixel_utils.c
  ${MGL_ROOT}/MGL/src/program.c
  ${MGL_ROOT}/MGL/src/rendering.c
  ${MGL_ROOT}/MGL/src/samplers.c
  ${MGL_ROOT}/MGL/src/shaders.c
  ${MGL_ROOT}/MGL/src/state.c
  ${MGL_ROOT}/MGL/src/tex_param.c
  ${MGL_ROOT}/MGL/src/textures.c
  ${MGL_ROOT}/MGL/src/uniforms.c
  ${MGL_ROOT}/MGL/src/utils.c
  ${MGL_ROOT}/MGL/src/vertex_arrays.c
  ${MGL_ROOT}/MGL/src/vertex_buffers.c
  ${CMAKE_CURRENT_LIST_DIR}/glm_params_ios.c   # replaces MGL/src/glm_params.c (CGL dlopen)
)

set(MGL_OBJC_SOURCES
  ${MGL_ROOT}/MGL/src/MGLRenderer.m
  ${MGL_ROOT}/MGL/src/MGLTextures.m
  ${MGL_ROOT}/MGL/src/hash_table.m
)

set(MGL_CXX_SOURCES
  ${MGL_ROOT}/MGL/spirv_cross_c.cpp
)

add_library(mgl_partial STATIC ${MGL_C_SOURCES} ${MGL_OBJC_SOURCES} ${MGL_CXX_SOURCES})

target_include_directories(mgl_partial PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/shim
  ${MGL_ROOT}/MGL/include
  ${MGL_ROOT}/MGL/include/GL
  ${MGL_ROOT}/external/SPIRV-Cross
  ${MGL_ROOT}/external/SPIRV-Headers/include
  ${MGL_ROOT}/external/SPIRV-Tools/include
  ${MGL_ROOT}/external/glslang
  ${MGL_ROOT}/external/glslang/glslang/Include
  ${MGL_ROOT}/external/glslang/glslang/Public
  ${MGL_ROOT}/external/glslang/SPIRV
)

target_compile_definitions(mgl_partial PRIVATE
  MGL_GL_CORE
  ENABLE_OPT=0
  TARGET_OS_IPHONE=1
  SPIRV_CROSS_C_API_MSL=1
  SPIRV_CROSS_C_API_GLSL=1
  SPIRV_CROSS_C_API_CPP=1
  SPIRV_CROSS_C_API_REFLECT=1
)

set_source_files_properties(${MGL_OBJC_SOURCES} PROPERTIES
  COMPILE_OPTIONS "-fobjc-arc"
  LANGUAGE OBJC)

# Merge SPIRV-Cross backends into libmgl.a and stage all five archives.
add_custom_command(TARGET mgl_partial POST_BUILD
  COMMAND libtool -static -o ${MGL_STAGE}/libmgl.a $<TARGET_FILE:mgl_partial>
          ${MGL_STAGE}/spirv-cross/libspirv-cross-core.a
          ${MGL_STAGE}/spirv-cross/libspirv-cross-msl.a
          ${MGL_STAGE}/spirv-cross/libspirv-cross-glsl.a
          ${MGL_STAGE}/spirv-cross/libspirv-cross-reflect.a
          ${MGL_STAGE}/spirv-cross/libspirv-cross-cpp.a
  COMMENT "Merged libmgl.a -> ${MGL_STAGE}/libmgl.a"
)
