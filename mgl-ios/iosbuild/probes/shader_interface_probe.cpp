// CPU-only reproduction of the MGL shader interface defect. Runs on macOS;
// neither CoreSimulator nor a Metal device is required. The negative control
// uses the old link sequence; the corrected sequence calls map_io before SPIR-V.
#include "../shader_interface.h"
#include <glslang/Public/resource_limits_c.h>
#include <spirv_msl.hpp>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

static const char* sources[] = {
    "#version 330 core\n"
    "layout(location=0) in vec3 position;\n"
    "layout(location=2) in vec2 texcoord;\n"
    "layout(location=3) in vec4 color;\n"
    "out vec2 tc; out vec4 cpri; out vec4 csec;\n"
    "void main() { tc=texcoord; cpri=color; csec=vec4(0); gl_Position=vec4(position,1); }\n",
    "#version 330 core\n"
    // Deliberately different declaration order: the linker must match by name.
    "in vec4 csec; in vec2 tc; in vec4 cpri;\n"
    "uniform sampler2D tex; out vec4 fragColor;\n"
    "void main() { fragColor=texture(tex,tc)*cpri+csec; }\n"
};

static void require(bool ok, const char* message)
{
    if (!ok) throw std::runtime_error(message);
}

static std::map<std::string, unsigned> locations(const std::vector<unsigned>& words)
{
    std::map<unsigned, std::string> names;
    std::map<unsigned, unsigned> decorations;
    for (size_t i = 5; i < words.size(); i += words[i] >> 16) {
        unsigned op = words[i] & 65535;
        if (op == spv::OpName) names[words[i + 1]] = reinterpret_cast<const char*>(&words[i + 2]);
        if (op == spv::OpDecorate && words[i + 2] == spv::DecorationLocation)
            decorations[words[i + 1]] = words[i + 3];
    }
    std::map<std::string, unsigned> result;
    for (const auto& decoration : decorations)
        result[names[decoration.first]] = decoration.second;
    return result;
}

static void run(bool mapIO, const std::filesystem::path& output, glslang_stage_t resolverStage = GLSLANG_STAGE_VERTEX)
{
    glslang_program_t* program = glslang_program_create();
    glslang_shader_t* shaders[2];
    for (int stage = 0; stage < 2; ++stage) {
        glslang_input_t input = {};
        input.language = GLSLANG_SOURCE_GLSL;
        input.stage = stage ? GLSLANG_STAGE_FRAGMENT : GLSLANG_STAGE_VERTEX;
        input.client = GLSLANG_CLIENT_OPENGL;
        // Match MGL's legacy-330 compilation settings, including its numeric
        // client version, to isolate the missing link step.
        input.client_version = static_cast<glslang_target_client_version_t>(330);
        input.target_language = GLSLANG_TARGET_SPV;
        input.target_language_version = GLSLANG_TARGET_SPV_1_0;
        input.code = sources[stage];
        input.default_version = 330;
        input.default_profile = GLSLANG_CORE_PROFILE;
        input.force_default_version_and_profile = 1;
        input.messages = static_cast<glslang_messages_t>(GLSLANG_MSG_DEBUG_INFO_BIT | GLSLANG_MSG_RELAXED_ERRORS_BIT);
        input.resource = glslang_default_resource();
        shaders[stage] = glslang_shader_create(&input);
        glslang_shader_set_options(shaders[stage], GLSLANG_SHADER_VULKAN_RULES_RELAXED | GLSLANG_SHADER_AUTO_MAP_LOCATIONS);
        require(glslang_shader_preprocess(shaders[stage], &input), glslang_shader_get_info_log(shaders[stage]));
        require(glslang_shader_parse(shaders[stage], &input), glslang_shader_get_info_log(shaders[stage]));
        glslang_program_add_shader(program, shaders[stage]);
    }
    require(glslang_program_link(program, GLSLANG_MSG_DEFAULT_BIT), glslang_program_get_info_log(program));
    if (mapIO) require(sharMapShaderInterface(program, resolverStage), glslang_program_get_info_log(program));

    std::map<std::string, unsigned> stageLocations[2];
    for (int stage = 0; stage < 2; ++stage) {
        glslang_program_SPIRV_generate(program, stage ? GLSLANG_STAGE_FRAGMENT : GLSLANG_STAGE_VERTEX);
        std::vector<unsigned> words(glslang_program_SPIRV_get_size(program));
        glslang_program_SPIRV_get(program, words.data());
        stageLocations[stage] = locations(words);
        spirv_cross::CompilerMSL compiler(words);
        std::string metal = compiler.compile();
        std::string name = std::string(mapIO ? "mapped-" : "legacy-") + (stage ? "fragment" : "vertex");
        std::ofstream(output / (name + ".metal")) << metal;
        std::ofstream binary(output / (name + ".spv"), std::ios::binary);
        binary.write(reinterpret_cast<const char*>(words.data()), words.size() * sizeof(unsigned));
        for (const char* varying : {"tc", "cpri", "csec"}) {
            const auto& locs = stageLocations[stage];
            std::printf("%s %s location=%s\n", name.c_str(), varying,
                        locs.count(varying) ? std::to_string(locs.at(varying)).c_str() : "MISSING");
            require(locs.count(varying) == (mapIO ? 1u : 0u), "Unexpected varying location state");
        }
        if (mapIO) {
            for (int i = 0; i < 3; ++i)
                require(metal.find("[[user(locn" + std::to_string(i) + ")]]") != std::string::npos,
                        "Metal varying binding missing");
            if (stage) require(metal.find("[[color(0)]]") != std::string::npos, "Metal color output binding missing");
        }
    }
    if (mapIO) {
        for (const char* varying : {"tc", "cpri", "csec"})
            require(stageLocations[0].at(varying) == stageLocations[1].at(varying), "Vertex/fragment location mismatch");
        require(stageLocations[0].at("tc") != stageLocations[0].at("cpri") &&
                stageLocations[0].at("tc") != stageLocations[0].at("csec") &&
                stageLocations[0].at("cpri") != stageLocations[0].at("csec"), "Varying location collision");
    }
    glslang_program_delete(program);
    for (auto* shader : shaders) glslang_shader_delete(shader);
}

int main(int argc, char** argv)
{
    try {
        std::filesystem::path output = argc > 1 ? argv[1] : "shader-interface-output";
        std::filesystem::create_directories(output);
        require(glslang_initialize_process(), "glslang initialization failed");
        run(false, output);
        run(true, output);
        std::filesystem::create_directories(output / "fragment-resolver");
        run(true, output / "fragment-resolver", GLSLANG_STAGE_FRAGMENT);
        glslang_finalize_process();
        std::puts("PASS: missing legacy bindings reproduced; mapped SPIR-V and Metal interfaces match.");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
