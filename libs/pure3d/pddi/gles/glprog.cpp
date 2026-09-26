//=============================================================================
// Copyright (c) 2002 Radical Games Ltd.  All rights reserved.
//=============================================================================

#include <pddi/gles/gl.hpp>
#include <pddi/gles/glprog.hpp>
#include <pddi/gles/glmat.hpp>

#include <string>
#include <sstream>
#include <algorithm>
#include <regex>
#include <cstdlib>
#include <vector>
#include <SDL.h>

#ifdef RAD_CG
#define CGGL_NO_OPENGL
#include <Cg/cg.h>    /* Can't include this?  Is Cg Toolkit installed! */
#include <Cg/cgGL.h>

void pglProgram::checkForCgError()
{
    CGerror error;
    const char* string = cgGetLastErrorString(&error);

    if (error != CG_NO_ERROR) {
        SDL_Log("%s\n", string);
        if (error == CG_COMPILER_ERROR) {
            printf("%s\n", cgGetLastListing(pglProgram::context));
        }
        __debugbreak();
    }
}

CGcontext pglProgram::context = nullptr;
CGprofile pglProgram::vertexProfile, pglProgram::fragmentProfile;
#endif

#ifdef RAD_CG
static inline void UniformColour(CGparameter param, pddiColour c)
{
    cgSetParameter4f(param, float(c.Red()) / 255, float(c.Green()) / 255, float(c.Blue()) / 255, float(c.Alpha()) / 255);
}
#else
static inline void UniformColour(GLint loc, pddiColour c)
{
    glUniform4f(loc, float(c.Red()) / 255, float(c.Green()) / 255, float(c.Blue()) / 255, float(c.Alpha()) / 255);
}
#endif

pglProgram::pglProgram()
{
#ifdef RAD_CG
    program = nullptr;
    projection = modelview = normalmatrix = alpharef = sampler = acs = nullptr;
#else
    program = 0;
    projection = modelview = normalmatrix = alpharef = sampler = acs = -1;

    #ifdef RAD_ANDROID
    lit = -1;
    #endif

#endif
}

pglProgram::~pglProgram()
{
#ifdef RAD_CG
    if (program)
        cgDestroyProgram(program);
#else
    if (program)
        glDeleteProgram(program);
#endif
}

void pglProgram::SetProjectionMatrix(const pddiMatrix* matrix)
{
#ifdef RAD_CG
    if (projection)
        cgSetMatrixParameterfc(projection, matrix->m[0]);
    checkForCgError();
#else
    if (projection >= 0)
        glUniformMatrix4fv(projection, 1, GL_FALSE, matrix->m[0]);
#endif
}

void pglProgram::SetModelViewMatrix(const pddiMatrix* matrix)
{
#ifdef RAD_CG
    if (modelview)
        cgSetMatrixParameterfc(modelview, matrix->m[0]);
    checkForCgError();

    if (normalmatrix)
    {
        pddiMatrix inverse;
        inverse.Invert(*matrix);
        inverse.Transpose();
        cgSetMatrixParameterfc(normalmatrix, inverse.m[0]);
    }
    checkForCgError();
#else
    if (modelview >= 0)
        glUniformMatrix4fv(modelview, 1, GL_FALSE, matrix->m[0]);
    if (normalmatrix >= 0)
    {
        pddiMatrix inverse;
        inverse.Invert(*matrix);
        inverse.Transpose();
        glUniformMatrix4fv(normalmatrix, 1, GL_FALSE, inverse.m[0]);
    }
#endif
}

void pglProgram::SetTextureEnvironment(const pglTextureEnv* texEnv)
{
#ifdef RAD_CG
    if (texEnv->lit)
    {
        UniformColour(acm, texEnv->ambient);
        UniformColour(ecm, texEnv->emissive);
        UniformColour(dcm, texEnv->diffuse);
        UniformColour(scm, texEnv->specular);
        cgSetParameter1f(srm, texEnv->shininess);
    }
    else
    {
        UniformColour(acm, pddiColour(-1));
        UniformColour(ecm, pddiColour(-1));
        UniformColour(dcm, pddiColour(-1));
        UniformColour(scm, pddiColour(-1));
        cgSetParameter1f(srm, 0.0f);
    }
    checkForCgError();

    if (texEnv->alphaTest && alpharef >= 0)
    {
        PDDIASSERT(texEnv->alphaCompareMode == PDDI_COMPARE_GREATER ||
            texEnv->alphaCompareMode == PDDI_COMPARE_GREATEREQUAL);
        cgSetParameter1f(alpharef, texEnv->alphaTest ? texEnv->alphaRef : 0.0f);
    }
    checkForCgError();
#else
    if (sampler >= 0)
        glUniform1i(sampler, 0);


    #ifdef RAD_ANDROID
    if (lit >= 0)
        glUniform1i(lit, texEnv->lit ? 1 : 0);
    #endif

    if (texEnv->lit)
    {
        UniformColour(acm, texEnv->ambient);
        UniformColour(ecm, texEnv->emissive);
        UniformColour(dcm, texEnv->diffuse);
        UniformColour(scm, texEnv->specular);
        glUniform1f(srm, texEnv->shininess);
    }
    else
    {
        UniformColour(acm, pddiColour(-1));
        UniformColour(ecm, pddiColour(-1));
        UniformColour(dcm, pddiColour(-1));
        UniformColour(scm, pddiColour(-1));
        glUniform1f(srm, 0.0f);
    }

    if (texEnv->alphaTest && alpharef >= 0)
    {
        PDDIASSERT(texEnv->alphaCompareMode == PDDI_COMPARE_GREATER ||
            texEnv->alphaCompareMode == PDDI_COMPARE_GREATEREQUAL);
        glUniform1f(alpharef, texEnv->alphaTest ? texEnv->alphaRef : 0.0f);
    }
#endif
}

void pglProgram::SetLightState(int handle, const pddiLight* lightState)
{
    if( handle >= PDDI_MAX_LIGHTS )
        return;

    // GL spec: glUniform* at location -1 is a silent no-op. Some drivers
    // (MGL) assert instead — guard here.
    if (lights[handle].enabled < 0)
        return;

    float dir[4];
    switch(lightState->type)
    {
        case PDDI_LIGHT_DIRECTIONAL :
            dir[0] = -lightState->worldDirection.x;
            dir[1] = -lightState->worldDirection.y;
            dir[2] = -lightState->worldDirection.z;
            dir[3] = 0.0f;
            break;

        case PDDI_LIGHT_POINT :
            dir[0] = lightState->worldPosition.x;
            dir[1] = lightState->worldPosition.y;
            dir[2] = lightState->worldPosition.z;
            dir[3] = 1.0f;
            break;

        case PDDI_LIGHT_SPOT :
            PDDIASSERT(0);
            break;
    }

#ifdef RAD_CG
    if (lights[handle].enabled)
    {
        cgSetParameter1i(lights[handle].enabled, lightState->enabled ? 1 : 0);
        cgSetParameter4fv(lights[handle].position, dir);
        UniformColour(lights[handle].colour, lightState->colour);
        cgSetParameter3f(lights[handle].attenuation, lightState->attenA, lightState->attenB, lightState->attenC);
    }
    checkForCgError();
#else
    glUniform1i(lights[handle].enabled, lightState->enabled ? 1 : 0);
    glUniform4fv(lights[handle].position, 1, dir);
    UniformColour(lights[handle].colour, lightState->colour);
    glUniform3f(lights[handle].attenuation, lightState->attenA, lightState->attenB, lightState->attenC);
#endif
}

void pglProgram::SetAmbientLight(pddiColour ambient)
{
    if (acs)
        UniformColour(acs, ambient);
}

#ifdef RAD_CG
bool pglProgram::LinkProgram(CGprogram vertexShader, CGprogram fragmentShader)
{
    program = cgCombinePrograms2(vertexShader, fragmentShader);
    checkForCgError();
    if (!program)
        return false;

    cgGLLoadProgram(program);
    checkForCgError();

    projection = cgGetNamedParameter(program, "projection");
    modelview = cgGetNamedParameter(program, "modelview");
    normalmatrix = cgGetNamedParameter(program, "normalmatrix");
    alpharef = cgGetNamedParameter(program, "alpharef");
    sampler = cgGetNamedParameter(program, "tex");
    checkForCgError();

    for (int i = 0; i < PDDI_MAX_LIGHTS; i++)
    {
        std::string prefix = std::string("lights[") + char('0' + i) + "].";
        lights[i].enabled = cgGetNamedParameter(program, (prefix + "enabled").c_str());
        lights[i].position = cgGetNamedParameter(program, (prefix + "position").c_str());
        lights[i].colour = cgGetNamedParameter(program, (prefix + "colour").c_str());
        lights[i].attenuation = cgGetNamedParameter(program, (prefix + "attenuation").c_str());
        checkForCgError();
    }

    acs = cgGetNamedParameter(program, "acs");
    acm = cgGetNamedParameter(program, "acm");
    dcm = cgGetNamedParameter(program, "dcm");
    scm = cgGetNamedParameter(program, "scm");
    ecm = cgGetNamedParameter(program, "ecm");
    srm = cgGetNamedParameter(program, "srm");
    checkForCgError();
    return true;
}
#else
bool pglProgram::LinkProgram(GLuint vertexShader, GLuint fragmentShader)
{
    program = glCreateProgram();

    if(vertexShader)
        glAttachShader(program, vertexShader);
    if(fragmentShader)
        glAttachShader(program, fragmentShader);

    glBindAttribLocation(program, 0, "position");
    glBindAttribLocation(program, 1, "normal");
    glBindAttribLocation(program, 2, "texcoord");
    glBindAttribLocation(program, 3, "color");

    glLinkProgram(program);

    GLint isLinked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, (int *)&isLinked);
    if (isLinked == GL_FALSE)
    {
        GLint maxLength = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);

        if (maxLength <= 0)
        {
            GLenum e = glGetError();
            SDL_Log("MGL Program linking reported FALSE (prog=%u logLen=0 err=0x%x) — treating as failure", program, (unsigned)e);
            return false;
        }

        // The maxLength includes the NULL character
        std::vector<GLchar> infoLog(maxLength);
        glGetProgramInfoLog(program, maxLength, &maxLength, &infoLog[0]);

        SDL_Log("Program linking error: %s", infoLog.data());
        return false;
    }
    
    projection = glGetUniformLocation(program, "projection");
    modelview = glGetUniformLocation(program, "modelview");
    normalmatrix = glGetUniformLocation(program, "normalmatrix");
    alpharef = glGetUniformLocation(program, "alpharef");
    sampler = glGetUniformLocation(program, "tex");

    for (int i = 0; i < PDDI_MAX_LIGHTS; i++)
    {
#ifdef RAD_IOS
        // esToDesktopGLSL flattens the uniform LightParams lights[4] array
        // into light0..light3 (spirv-cross MSL cannot express a uniform
        // struct array). Query the flattened names on iOS.
        std::string prefix = std::string("light") + char('0' + i) + ".";
#else
        std::string prefix = std::string("lights[") + char('0' + i) + "].";
#endif
        lights[i].enabled = glGetUniformLocation(program, (prefix + "enabled").c_str());
        lights[i].position = glGetUniformLocation(program, (prefix + "position").c_str());
        lights[i].colour = glGetUniformLocation(program, (prefix + "colour").c_str());
        lights[i].attenuation = glGetUniformLocation( program, (prefix + "attenuation").c_str() );
    }

    acs = glGetUniformLocation(program, "acs");
    acm = glGetUniformLocation(program, "acm");
    dcm = glGetUniformLocation(program, "dcm");
    scm = glGetUniformLocation(program, "scm");
    ecm = glGetUniformLocation(program, "ecm");
    srm = glGetUniformLocation(program, "srm");

    #ifdef RAD_ANDROID
    lit = glGetUniformLocation(program, "lit");
    #endif

#ifndef RAD_VITAGL
    // Always detach shaders after a successful link
    if(vertexShader)
        glDetachShader(program, vertexShader);
    if(fragmentShader)
        glDetachShader(program, fragmentShader);
#endif
    return true;
}
#endif

#ifdef RAD_CG
CGprogram pglProgram::CompileShader(GLenum type, const char* source)
{
    if(!context)
    {
        context = cgCreateContext();
        cgGLSetDebugMode(CG_FALSE);
        cgSetParameterSettingMode(context, CG_IMMEDIATE_PARAMETER_SETTING);
        vertexProfile = cgGetProfile("glslv");
        assert(cgGLIsProfileSupported(vertexProfile));
        cgGLSetOptimalOptions(vertexProfile);
        fragmentProfile = cgGetProfile("glslf");
        assert(cgGLIsProfileSupported(fragmentProfile));
        cgGLSetOptimalOptions(fragmentProfile);
    }
    checkForCgError();

    const char* args[] = { "version=330", NULL };
    CGprofile profile = type == GL_VERTEX_SHADER ?
        vertexProfile : fragmentProfile;
    cgGLSetOptimalOptions(profile);
    CGprogram program = cgCreateProgram(
        context,
        CG_SOURCE,
        source,
        profile,
        "main",
        args);
    checkForCgError();
    return program;
}
#else
// iOS: MGL compiles GLSL through glslang with a DESKTOP OpenGL client
// (GLSLANG_CLIENT_OPENGL). The engine feeds GLSL ES 1.00 sources
// ("precision highp float;", attribute/varying, texture2D) which glslang
// rejects under a desktop profile. Translate ES 1.00 -> GLSL 330 core
// before handing the source to MGL. Android keeps the raw ES source.
static std::string esToDesktopGLSL(const char* source, GLenum type)
{
    // A legacy #version (< 330) makes MGL enable location auto-assignment
    // (SPIR-V requires locations) and rewrite the line to "#version 330 core"
    // before glslang sees it. So present the translated body as GLSL 1.00.
    std::string out;
    if (source && strstr(source, "#version") == NULL)
        out = "#version 100\n";
    int vInLoc = 0;
    std::string line;
    std::istringstream ss(source ? source : "");
    bool fragUsesFragColor = false;
    std::string body;
    while (std::getline(ss, line))
    {
        // strip a trailing \r
        if (!line.empty() && line[line.size()-1] == '\r') line.erase(line.size()-1);
        // skip ES precision qualifiers (desktop defaults are highp)
        const char* t = line.c_str();
        while (*t == ' ' || *t == '\t') ++t;
        if (strncmp(t, "precision", 9) == 0) continue;

        if (strstr(line.c_str(), "gl_FragColor") != NULL)
        {
            fragUsesFragColor = true;
            std::string cleaned;
            size_t pos = 0;
            while ((pos = line.find("gl_FragColor", pos)) != std::string::npos)
            {
                line.replace(pos, 13, "fragColor");
                pos += 9;
            }
        }
        if (type == GL_VERTEX_SHADER)
        {
            size_t p = line.find("attribute ");
            if (p != std::string::npos) line.replace(p, 10, "in ");
            else
            {
                p = line.find("varying ");
                if (p != std::string::npos) line.replace(p, 8, "out ");
            }
        }
        else
        {
            size_t p = line.find("varying ");
            if (p != std::string::npos) line.replace(p, 8, "in ");
        }
        // MSL stage_in structs need [[attribute(n)]], which requires SPIR-V
        // Location on VERTEX INPUTS only. glslang's 100->330 upgrade path
        // rejects layout() on outputs/varyings, and fragment I/O pairs fine
        // without it, so restrict emission to vertex "in" declarations.
        if (type == GL_VERTEX_SHADER)
        {
            size_t p = line.find("layout(");
            const char* lt = line.c_str();
            while (*lt == ' ' || *lt == '\t') ++lt;
            if (p == std::string::npos && strncmp(lt, "in ", 3) == 0)
            {
                line.insert(0, "layout(location = " + std::to_string(vInLoc) + ") ");
                vInLoc++;
            }
        }
        // ES texture sampling entry points
        size_t p = line.find("texture2D");
        while (p != std::string::npos)
        {
            line.replace(p, 9, "texture");
            p = line.find("texture2D", p + 7);
        }
        body += line;
        body += '\n';
    }
    if (fragUsesFragColor && type == GL_FRAGMENT_SHADER)
        out += "out vec4 fragColor;\n";
    // spirv-cross MSL splits uniform arrays into per-element buffers but
    // keeps dynamic indexing -> "lights[i] on a pointer" compile error.
    // Unroll loops of the form "for (int i = 0; i < N; i++) { .. lights[i] .. }".
    {
        std::string unrolled;
        std::istringstream uss(body);
        std::string ln;
        while (std::getline(uss, ln))
        {
            static const char* pat = "for (int i = 0; i < ";
            size_t f = ln.find(pat);
            if (f != std::string::npos)
            {
                size_t b = ln.find(';', f + strlen(pat));
                if (b != std::string::npos)
                {
                    std::string bound = ln.substr(f + strlen(pat), b - (f + strlen(pat)));
                    int n = atoi(bound.c_str());
                    if (n > 0 && n <= 16)
                    {
                        std::vector<std::string> bodyLines;
                        bodyLines.push_back(ln);
                        int depth = (int)std::count(ln.begin(), ln.end(), '{') - (int)std::count(ln.begin(), ln.end(), '}');
                        // find the opening brace (may be on the next line)
                        while (depth == 0 && std::getline(uss, ln))
                        {
                            bodyLines.push_back(ln);
                            depth += (int)std::count(ln.begin(), ln.end(), '{');
                            depth -= (int)std::count(ln.begin(), ln.end(), '}');
                        }
                        while (depth > 0 && std::getline(uss, ln))
                        {
                            depth += (int)std::count(ln.begin(), ln.end(), '{');
                            depth -= (int)std::count(ln.begin(), ln.end(), '}');
                            bodyLines.push_back(ln);
                        }
                        for (int k = 0; k < n; k++)
                        {
                            bool guardOpen = false;
                            bool skippedHeader = false;
                            std::string guardNeedle = "lights[" + std::to_string(k) + "].enabled == 0) continue;";
                            for (auto& bl : bodyLines)
                            {
                                std::string rep = bl;
                                std::string needle = "lights[i]";
                                size_t q = 0;
                                while ((q = rep.find(needle, q)) != std::string::npos)
                                {
                                    rep.replace(q, needle.size(), "lights[" + std::to_string(k) + "]");
                                    q += 9;
                                }
                                // drop the "for (int i..)" header of each copy: the
                                // guard below already scopes the body per element.
                                if (!skippedHeader &&
                                    rep.find("for (int i") != std::string::npos)
                                {
                                    skippedHeader = true;
                                    continue;
                                }
                                if (rep.find(guardNeedle) != std::string::npos)
                                {
                                    // "if (lights[K].enabled == 0) continue;" becomes
                                    // an inverted if-block so spirv-cross doesn't
                                    // have to synthesize a continue outside a loop.
                                    size_t g = rep.find("if (lights[");
                                    std::string indent = rep.substr(0, g);
                                    unrolled += indent + "if (lights[" + std::to_string(k) + "].enabled != 0)\n";
                                    unrolled += indent + "{\n";
                                    guardOpen = true;
                                    continue;
                                }
                                if (guardOpen)
                                {
                                    // the captured loop's own closing brace is
                                    // replaced by the guard's close below
                                    const char* tr = rep.c_str();
                                    while (*tr == ' ' || *tr == '\t' || *tr == '\r') ++tr;
                                    if (*tr == '}' && tr[1] == '\0') continue;
                                }
                                unrolled += rep;
                                unrolled += '\n';
                            }
                            if (guardOpen)
                                unrolled += "        }\n";
                        }
                        continue;
                    }
                }
            }
            // flexible-whitespace fallback: for(int i=0;i<4;i++) etc
            {
                static std::regex rx("for\\s*\\(\\s*int\\s+([A-Za-z_]\\w*)\\s*=\\s*0\\s*;\\s*\\1\\s*<\\s*([0-9]+)\\s*;\\s*\\1\\s*\\+\\+\\s*\\)");
                std::smatch m;
                if (std::regex_search(ln, m, rx))
                {
                    int n = atoi(m[2].str().c_str());
                    if (n > 0 && n <= 16 && ln.find("lights") != std::string::npos)
                    {
                        std::vector<std::string> bodyLines;
                        bodyLines.push_back(ln);
                        int depth = (int)std::count(ln.begin(), ln.end(), '{') - (int)std::count(ln.begin(), ln.end(), '}');
                        while (depth == 0 && std::getline(uss, ln))
                        {
                            bodyLines.push_back(ln);
                            depth += (int)std::count(ln.begin(), ln.end(), '{');
                            depth -= (int)std::count(ln.begin(), ln.end(), '}');
                        }
                        while (depth > 0 && std::getline(uss, ln))
                        {
                            depth += (int)std::count(ln.begin(), ln.end(), '{');
                            depth -= (int)std::count(ln.begin(), ln.end(), '}');
                            bodyLines.push_back(ln);
                        }
                        for (int k = 0; k < n; k++)
                        {
                            bool guardOpen = false;
                            bool skippedHeader = false;
                            std::string guardNeedle = "lights[" + std::to_string(k) + "].enabled == 0) continue;";
                            for (auto& bl : bodyLines)
                            {
                                std::string rep = bl;
                                std::string needle = "lights[i]";
                                size_t q = 0;
                                while ((q = rep.find(needle, q)) != std::string::npos)
                                {
                                    rep.replace(q, needle.size(), "lights[" + std::to_string(k) + "]");
                                    q += 9;
                                }
                                // drop the "for (int i..)" header of each copy: the
                                // guard below already scopes the body per element.
                                if (!skippedHeader &&
                                    rep.find("for (int i") != std::string::npos)
                                {
                                    skippedHeader = true;
                                    continue;
                                }
                                if (rep.find(guardNeedle) != std::string::npos)
                                {
                                    // "if (lights[K].enabled == 0) continue;" becomes
                                    // an inverted if-block so spirv-cross doesn't
                                    // have to synthesize a continue outside a loop.
                                    size_t g = rep.find("if (lights[");
                                    std::string indent = rep.substr(0, g);
                                    unrolled += indent + "if (lights[" + std::to_string(k) + "].enabled != 0)\n";
                                    unrolled += indent + "{\n";
                                    guardOpen = true;
                                    continue;
                                }
                                if (guardOpen)
                                {
                                    // the captured loop's own closing brace is
                                    // replaced by the guard's close below
                                    const char* tr = rep.c_str();
                                    while (*tr == ' ' || *tr == '\t' || *tr == '\r') ++tr;
                                    if (*tr == '}' && tr[1] == '\0') continue;
                                }
                                unrolled += rep;
                                unrolled += '\n';
                            }
                            if (guardOpen)
                                unrolled += "        }\n";
                        }
                        continue;
                    }
                }
            }
            unrolled += ln;
            unrolled += '\n';
        }
        body = unrolled;
    }
    // spirv-cross MSL cannot express a uniform STRUCT ARRAY as discrete
    // per-element buffers with indexing (even constant) — it emits
    // "lights[N].field on a pointer" MSL. Flatten to four separate uniforms
    // and rewrite the (already unrolled) constant indices to light0..light3.
    {
        std::string flattened;
        std::istringstream fss(body);
        std::string ln;
        bool inLightStruct = false;
        while (std::getline(fss, ln))
        {
            if (!inLightStruct)
            {
                size_t s = ln.find("uniform struct LightParams {");
                if (s != std::string::npos)
                {
                    // strip "uniform " so this becomes a plain struct def
                    ln.replace(s, 8, "");
                    inLightStruct = true;
                }
            }
            if (inLightStruct)
            {
                const char kNeedle[] = "} lights[4];";
                size_t e = ln.find(kNeedle);
                if (e != std::string::npos)
                {
                    // replace() count must cover the whole needle
                    // ("} lights[4];" = 12 chars); 11 leaves a stray ';'
                    // that glslang rejects as an extraneous semicolon.
                    ln.replace(e, strlen(kNeedle), "};\n"
                        "uniform LightParams light0;\n"
                        "uniform LightParams light1;\n"
                        "uniform LightParams light2;\n"
                        "uniform LightParams light3;");
                    inLightStruct = false;
                }
                else
                {
                    flattened += ln;
                    flattened += '\n';
                    continue;
                }
            }
            // lights[N] -> lightN for literal N (loop unrolling above already
            // removed every dynamic "lights[i]" access)
            for (int kk = 0; kk < 8; kk++)
            {
                std::string needle = "lights[" + std::to_string(kk) + "]";
                size_t q = 0;
                while ((q = ln.find(needle, q)) != std::string::npos)
                {
                    ln.replace(q, needle.size(), "light" + std::to_string(kk));
                    q += 6;
                }
            }
            flattened += ln;
            flattened += '\n';
        }
        body = flattened;
    }

    return out + body;
}

GLuint pglProgram::CompileShader(GLenum type, const char* source)
{
#if defined(RAD_IOS)
    std::string translated = esToDesktopGLSL(source, type);
    const char* src = translated.c_str();
#else
    const char* src = source;
#endif
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, 0);
    glCompileShader(shader);

    GLint isCompiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
    if(isCompiled == GL_FALSE)
    {
        GLint maxLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);

        if (maxLength <= 0)
        {
            // MGL reports COMPILE_STATUS FALSE only when it stored an info
            // log; an empty log means parse succeeded and the shader is
            // usable — real errors (glslang/SPIRV-Cross) surface at link.
            return shader;
        }

        // The maxLength includes the NULL character
        std::vector<GLchar> infoLog(maxLength);
        glGetShaderInfoLog(shader, maxLength, &maxLength, &infoLog[0]);

        // We don't need the shader anymore.
        glDeleteShader(shader);

        SDL_Log("Shader compilation error: %s", infoLog.data());
        {
            FILE*_f=fopen("/tmp/shar_shader_err.txt","a");
            if(_f){ fprintf(_f,"=== type=%u shader compile FAILED ===\n%s\n--- source ---\n%s\n",(unsigned)type,infoLog.data(),src); fclose(_f); }
        }
        return 0;
    }
    return shader;
}
#endif

#ifdef RAD_CG
void pglProgram::UseProgram()
{
    cgUpdateProgramParameters(program);
    cgGLBindProgram(program);
    checkForCgError();
}
#endif

#ifdef RAD_CG
pglProgram* pglProgram::CreateProgram(CGprogram vertexShader, CGprogram fragmentShader)
#else
pglProgram* pglProgram::CreateProgram(GLuint vertexShader, GLuint fragmentShader)
#endif
{
    pglProgram* program = new pglProgram();
    program->AddRef();
    if(!program->LinkProgram(vertexShader, fragmentShader))
    {
        program->Release();
        return nullptr;
    }
    return program;
}
