//=============================================================================
// Copyright (c) 2002 Radical Games Ltd.  All rights reserved.
//=============================================================================

#include <pddi/gles/gl.hpp>
#include <pddi/gles/glcon.hpp>
#include <pddi/gles/gldisplay.hpp>
#include <pddi/base/debug.hpp>
#include <SDL.h>
#if defined(RAD_IOS)
#include <SDL_metal.h>
#endif

#include<stdio.h>
#include<string.h>
#include<math.h>

#if defined(RAD_IOS)
// bridge (iosdocuments.mm): tamano en pixeles del drawable principal.
extern "C" void iOSGetScreenPixelSize(int* outW, int* outH);
extern "C" const char* iOSGetDocumentsDir(void);
extern "C" void iOSRecordGameFrame(void);

// --- MGL (OpenGL-on-Metal): the GL implementation on iOS 26/27 (EAGL
// drawables are dead there). MGL exports the gl* API + C context funcs;
// glad resolves gl* through dlsym against this library.
#include <dlfcn.h>
typedef struct GLMContextRec_t* GLMContext;
extern "C" {
    GLMContext createGLMContext(unsigned int format, unsigned int type,
                                unsigned int depth_format, unsigned int depth_type,
                                unsigned int stencil_format, unsigned int stencil_type);
    void MGLsetCurrentContext(GLMContext ctx);
    void MGLswapBuffers(GLMContext ctx);
    void* CppCreateMGLRendererAndBindToUIView(void *glm_ctx, void *uiview, float contentsScale);
}
static GLMContext s_iosMGLContext = NULL;
static bool s_iosMGLInited = false;

static void* iOSMGLGetProcAddress(const char* name)
{
    // MGL is linked statically into the main executable. dlsym(RTLD_DEFAULT)
    // can resolve gl* to a simulator system stub (stable 0x248.. region) that
    // returns 0 for object creation; the main-program handle finds MGL first.
    static void* mainHandle = NULL;
    if (mainHandle == NULL)
        mainHandle = dlopen(NULL, RTLD_NOW | RTLD_LOCAL);
    void* p = mainHandle ? dlsym(mainHandle, name) : NULL;
    if (p == NULL)
        p = dlsym(RTLD_DEFAULT, name);
    return p;
}

static bool iOSMGLStart(SDL_Window* win)
{
    if (s_iosMGLInited)
        return true;
    SDL_MetalView mv = SDL_Metal_CreateView(win);
    if (mv == NULL)
    {
        SDL_Log("MGL: SDL_Metal_CreateView failed: %s", SDL_GetError());
        return false;
    }
    int px = 0, py = 0, wx = 0, wy = 0;
    iOSGetScreenPixelSize(&px, &py);
    SDL_GetWindowSize(win, &wx, &wy);
    float scale = (wx > 0) ? (float)px / (float)wx : 3.0f;
    s_iosMGLContext = createGLMContext(0x1907 /*GL_BGRA*/,
                                       0x8368 /*GL_UNSIGNED_INT_8_8_8_8_REV*/,
                                       0x1902 /*GL_DEPTH_COMPONENT*/,
                                       0x1406 /*GL_FLOAT*/, 0, 0);
    if (s_iosMGLContext == NULL)
    {
        SDL_Log("MGL: createGLMContext failed");
        return false;
    }
    MGLsetCurrentContext(s_iosMGLContext);
    void* renderer = CppCreateMGLRendererAndBindToUIView(
        (void*)s_iosMGLContext, mv, scale);
    if (renderer == NULL)
    {
        SDL_Log("MGL: renderer bind failed");
        return false;
    }
    s_iosMGLInited = true;
    SDL_Log("MGL: OpenGL-on-Metal initialized (scale %.1f)", scale);
    return true;
}
#endif

#if defined(RAD_ANDROID)
#include <jni.h>

static int gSHARAndroidRenderWidth = 0;
static int gSHARAndroidRenderHeight = 0;

static int gSHARLastLoggedDisplayWidth = 0;
static int gSHARLastLoggedDisplayHeight = 0;

extern "C" JNIEXPORT void JNICALL
Java_org_libsdl_app_SDLSurface_nativeSetSHARRenderResolution(
    JNIEnv* env,
    jclass clazz,
    jint width,
    jint height
)
{
    if (width <= 0 || height <= 0)
        return;

    const bool changed =
        gSHARAndroidRenderWidth != (int)width ||
        gSHARAndroidRenderHeight != (int)height;

    gSHARAndroidRenderWidth = (int)width;
    gSHARAndroidRenderHeight = (int)height;

    if (changed) {
        SDL_Log(
            "SHAR Android Java render resolution received: %dx%d",
            gSHARAndroidRenderWidth,
            gSHARAndroidRenderHeight
        );
    }
}

static bool
GetSHARAndroidRenderResolution(int* width, int* height)
{
    if (width == NULL || height == NULL)
        return false;

    if (gSHARAndroidRenderWidth <= 0 || gSHARAndroidRenderHeight <= 0)
        return false;

    *width = gSHARAndroidRenderWidth;
    *height = gSHARAndroidRenderHeight;

    return true;
}

static void
LogSHARDisplayResolutionIfChanged(const char* reason, int width, int height)
{
    if (width <= 0 || height <= 0)
        return;

    if (gSHARLastLoggedDisplayWidth == width &&
        gSHARLastLoggedDisplayHeight == height)
        return;

    gSHARLastLoggedDisplayWidth = width;
    gSHARLastLoggedDisplayHeight = height;

    SDL_Log(
        "SHAR Android PDDI display resolution [%s]: %dx%d",
        reason ? reason : "unknown",
        width,
        height
    );
}
#endif

bool pglDisplay::CheckExtension( const char *extName )
{
    return SDL_GL_ExtensionSupported(extName);
}

pglDisplay ::pglDisplay(pddiDisplayInfo* info)
{
    displayInfo = info;
    mode = PDDI_DISPLAY_WINDOW;
    winWidth = 640;
    winHeight = 480;
    winBitDepth = 32;

    context = NULL;

    win = NULL;
    hRC = NULL;
    prevRC = NULL;

    extBGRA = false;

    gammaR = gammaG = gammaB = 1.0f;

    reset = true;
	m_ForceVSync = false;
}

pglDisplay ::~pglDisplay()
{
    /* release and free the device context and rendering context */
#if SDL_MAJOR_VERSION < 3
    SDL_GL_DeleteContext(hRC);
    SDL_SetWindowGammaRamp(win, initialGammaRamp[0], initialGammaRamp[1], initialGammaRamp[2]);
#else
    SDL_GL_DestroyContext((SDL_GLContext)hRC);
#endif
}

#define KEYPRESSED(x) (GetKeyState((x)) & (1<<(sizeof(int)*8)-1))

long pglDisplay ::ProcessWindowMessage(SDL_Window* win, const SDL_WindowEvent* event)
{
#if SDL_MAJOR_VERSION < 3
    switch(event->event)
    {
        case SDL_WINDOWEVENT_SIZE_CHANGED:
        {
#if defined(RAD_ANDROID)
            int renderW = 0;
            int renderH = 0;

            /*
             * On Android, Java/SDLSurface is the source of truth for the
             * internal render resolution. Do not let SDL report the physical
             * drawable size as the PDDI render size.
             */
            if (GetSHARAndroidRenderResolution(&renderW, &renderH)) {
                winWidth = renderW;
                winHeight = renderH;

                LogSHARDisplayResolutionIfChanged(
                    "window-size-changed-java",
                    winWidth,
                    winHeight
                );
            } else
#endif
            {
#if defined(RAD_IOS)
                iOSGetScreenPixelSize(&winWidth, &winHeight);
#else
    #if defined(RAD_IOS)
            iOSGetScreenPixelSize(&winWidth, &winHeight);
#else
            SDL_GL_GetDrawableSize(win, &winWidth, &winHeight);
#endif
#endif

#if defined(RAD_ANDROID)
                LogSHARDisplayResolutionIfChanged(
                    "window-size-changed-sdl-fallback",
                    winWidth,
                    winHeight
                );
#endif
            }
            break;
        }

        case SDL_WINDOWEVENT_CLOSE:
            /* release and free the device context and rendering context */
            SDL_GL_DeleteContext(hRC);
            break;

        default:
            return 0;
    }
#else
    switch(event->type)
    {
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        {
#if defined(RAD_ANDROID)
            int renderW = 0;
            int renderH = 0;

            /*
             * On Android, Java/SDLSurface is the source of truth for the
             * internal render resolution. Do not let SDL report the physical
             * pixel size as the PDDI render size.
             */
            if (GetSHARAndroidRenderResolution(&renderW, &renderH)) {
                winWidth = renderW;
                winHeight = renderH;

                LogSHARDisplayResolutionIfChanged(
                    "window-pixel-size-changed-java",
                    winWidth,
                    winHeight
                );
            } else
#endif
            {
                SDL_GetWindowSizeInPixels(win, &winWidth, &winHeight);

#if defined(RAD_ANDROID)
                LogSHARDisplayResolutionIfChanged(
                    "window-pixel-size-changed-sdl-fallback",
                    winWidth,
                    winHeight
                );
#endif
            }
            break;
        }

        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            /* release and free the device context and rendering context */
            SDL_GL_DestroyContext((SDL_GLContext)hRC);
            break;

        default:
            return 0;
    }
#endif

    /* return 1 if handled message, 0 if not */
    return 1;
}

void pglDisplay ::SetWindow(SDL_Window* wnd)
{
#if SDL_MAJOR_VERSION < 3
    SDL_GetWindowGammaRamp(wnd, initialGammaRamp[0], initialGammaRamp[1], initialGammaRamp[2]);
#endif
    win = wnd;
}

bool pglDisplay ::InitDisplay(int x, int y, int bpp)
{
    // check we are not trying to init to the same resolution
    if((x == winWidth) &&  (y == winHeight) && (bpp == winBitDepth))
    {
        return true;
    }

    // fill in the relevent portions of the casced display init structure
    displayInit.xsize = x;
    displayInit.ysize = y;
    displayInit.bpp = bpp;

    // do the full init
    return InitDisplay(&displayInit);
}

#ifdef RAD_DEBUG
void GLAPIENTRY
MessageCallback(GLenum source,
    GLenum type,
    GLuint id,
    GLenum severity,
    GLsizei length,
    const GLchar* message,
    const void* userParam)
{
    switch(severity)
    {
        case GL_DEBUG_SEVERITY_HIGH_KHR:
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", message);
            break;
        case GL_DEBUG_SEVERITY_MEDIUM_KHR:
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "%s", message);
            break;
        case GL_DEBUG_SEVERITY_LOW_KHR:
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "%s", message);
            break;
        case GL_DEBUG_SEVERITY_NOTIFICATION_KHR:
            SDL_LogVerbose(SDL_LOG_CATEGORY_APPLICATION, "%s", message);
            break;
    }
}
#endif

bool pglDisplay ::InitDisplay(const pddiDisplayInit* init)
{
    displayInit = *init;

    int x = init->xsize;
    int y = init->ysize;
    int bpp = init->bpp;
    pddiDisplayMode m = init->displayMode;
    int colourBufferCount = 1;
    unsigned bufMask = init->bufferMask;
    unsigned nSamples = 0;

    reset = true;

    mode = m;
#if defined(RAD_IOS)
    // iOS: el window ya es 100% fullscreen (UIKit lo fuerza). NO debemos fijar
    // un modo de display derivado de la resolucion pedida (800x600), porque eso
    // introduce letterboxing/backbuffer 4:3. SDL crea el drawable a pantalla
    // completa; solo usamos su tamaño real como viewport del PDDI.
#else
    SDL_DisplayMode displayMode = {}, closestMode = {};
    displayMode.w = x;
    displayMode.h = y;
#if SDL_MAJOR_VERSION < 3
    SDL_DisplayMode* pDisplayMode = SDL_GetClosestDisplayMode(displayInfo->id, &displayMode, &closestMode);
    if (pDisplayMode)
        SDL_SetWindowDisplayMode(win, pDisplayMode);
#else
    if(SDL_GetClosestFullscreenDisplayMode((SDL_DisplayID)displayInfo->id, x, y, 0.0f, false, &closestMode))
        SDL_SetWindowFullscreenMode(win, &closestMode);
#endif

    #ifndef __SWITCH__
    SDL_SetWindowFullscreen(win, mode == PDDI_DISPLAY_FULLSCREEN ? SDL_WINDOW_FULLSCREEN : 0);
    #endif
#endif

#if defined(RAD_ANDROID) || defined(__ANDROID__)
    /*
     * Android adaptive render resolution:
     *
     * SDLSurface.java has already calculated the internal Surface buffer size
     * and sent it through nativeSetSHARRenderResolution().
     *
     * Use that size as PDDI display size so pglContext::SetupHardwareProjection()
     * later uses the adaptive resolution for glViewport/projection/scissor.
     */
    {
        int renderW = 0;
        int renderH = 0;

        if (GetSHARAndroidRenderResolution(&renderW, &renderH)) {
            winWidth = renderW;
            winHeight = renderH;

            LogSHARDisplayResolutionIfChanged(
                "init-display-java",
                winWidth,
                winHeight
            );
        } else {
#if SDL_MAJOR_VERSION < 3
#if defined(RAD_IOS)
            iOSGetScreenPixelSize(&winWidth, &winHeight);
#else
            SDL_GL_GetDrawableSize(win, &winWidth, &winHeight);
#endif
#else
            SDL_GetWindowSizeInPixels(win, &winWidth, &winHeight);
#endif
            LogSHARDisplayResolutionIfChanged(
                "init-display-sdl-fallback",
                winWidth,
                winHeight
            );
        }
    }
#elif defined(RAD_IOS)
    // iOS: el framebuffer GL es a pantalla completa, pero SDL reporta el tamaño
    // logico del window (800x600) via SDL_GetWindowSize*. Usamos los pixeles
    // fisicos de la pantalla para que el viewport del PDDI llene la pantalla
    // (sin letterboxing). Se escala ese tamaño fisico a una resolucion interna
    // razonable en win32platform; aqui solo capturamos el fisico.
    {
        int physW = 0, physH = 0;
        iOSGetScreenPixelSize( &physW, &physH );
        if( physW > 0 && physH > 0 ) {
            winWidth  = physW;
            winHeight = physH;
            SDL_Log("PDDI iOS physical size set to %dx%d (requested %dx%d)", physW, physH, x, y);
        } else {
            #if SDL_MAJOR_VERSION < 3
            SDL_GL_GetDrawableSize( win, &winWidth, &winHeight );
            #else
            SDL_GetWindowSizeInPixels( win, &winWidth, &winHeight );
            #endif
        }
    }
#else
#if SDL_MAJOR_VERSION < 3
    SDL_GL_GetDrawableSize( win, &winWidth, &winHeight );
#else
    SDL_GetWindowSizeInPixels( win, &winWidth, &winHeight );
#endif
#endif

    winBitDepth = bpp;

    if (hRC)
        return true;

#if defined(RAD_IOS)
    // No EAGL on iOS 26/27: MGL provides the GL context over Metal. glad
    // resolves every gl* entry from MGL's exported implementations.
    if (!iOSMGLStart(win))
        return false;
    if (!gladLoadGLES2Loader((GLADloadproc)iOSMGLGetProcAddress))
    {
        // Their generated loader is unreliable with MGL's version/ext string
        // handling; assign the whole table directly from MGL's exports.
    glad_glAccum = (decltype(glad_glAccum))iOSMGLGetProcAddress("glAccum");
    glad_glActiveTexture = (decltype(glad_glActiveTexture))iOSMGLGetProcAddress("glActiveTexture");
    glad_glAlphaFunc = (decltype(glad_glAlphaFunc))iOSMGLGetProcAddress("glAlphaFunc");
    glad_glAreTexturesResident = (decltype(glad_glAreTexturesResident))iOSMGLGetProcAddress("glAreTexturesResident");
    glad_glArrayElement = (decltype(glad_glArrayElement))iOSMGLGetProcAddress("glArrayElement");
    glad_glAttachShader = (decltype(glad_glAttachShader))iOSMGLGetProcAddress("glAttachShader");
    glad_glBegin = (decltype(glad_glBegin))iOSMGLGetProcAddress("glBegin");
    glad_glBeginConditionalRender = (decltype(glad_glBeginConditionalRender))iOSMGLGetProcAddress("glBeginConditionalRender");
    glad_glBeginQuery = (decltype(glad_glBeginQuery))iOSMGLGetProcAddress("glBeginQuery");
    glad_glBeginTransformFeedback = (decltype(glad_glBeginTransformFeedback))iOSMGLGetProcAddress("glBeginTransformFeedback");
    glad_glBindAttribLocation = (decltype(glad_glBindAttribLocation))iOSMGLGetProcAddress("glBindAttribLocation");
    glad_glBindBuffer = (decltype(glad_glBindBuffer))iOSMGLGetProcAddress("glBindBuffer");
    glad_glBindBufferBase = (decltype(glad_glBindBufferBase))iOSMGLGetProcAddress("glBindBufferBase");
    glad_glBindBufferRange = (decltype(glad_glBindBufferRange))iOSMGLGetProcAddress("glBindBufferRange");
    glad_glBindFragDataLocation = (decltype(glad_glBindFragDataLocation))iOSMGLGetProcAddress("glBindFragDataLocation");
    glad_glBindFramebuffer = (decltype(glad_glBindFramebuffer))iOSMGLGetProcAddress("glBindFramebuffer");
    glad_glBindRenderbuffer = (decltype(glad_glBindRenderbuffer))iOSMGLGetProcAddress("glBindRenderbuffer");
    glad_glBindTexture = (decltype(glad_glBindTexture))iOSMGLGetProcAddress("glBindTexture");
    glad_glBindVertexArray = (decltype(glad_glBindVertexArray))iOSMGLGetProcAddress("glBindVertexArray");
    glad_glBindVertexArrayOES = (decltype(glad_glBindVertexArrayOES))iOSMGLGetProcAddress("glBindVertexArrayOES");
    glad_glBitmap = (decltype(glad_glBitmap))iOSMGLGetProcAddress("glBitmap");
    glad_glBlendColor = (decltype(glad_glBlendColor))iOSMGLGetProcAddress("glBlendColor");
    glad_glBlendEquation = (decltype(glad_glBlendEquation))iOSMGLGetProcAddress("glBlendEquation");
    glad_glBlendEquationSeparate = (decltype(glad_glBlendEquationSeparate))iOSMGLGetProcAddress("glBlendEquationSeparate");
    glad_glBlendFunc = (decltype(glad_glBlendFunc))iOSMGLGetProcAddress("glBlendFunc");
    glad_glBlendFuncSeparate = (decltype(glad_glBlendFuncSeparate))iOSMGLGetProcAddress("glBlendFuncSeparate");
    glad_glBlitFramebuffer = (decltype(glad_glBlitFramebuffer))iOSMGLGetProcAddress("glBlitFramebuffer");
    glad_glBufferData = (decltype(glad_glBufferData))iOSMGLGetProcAddress("glBufferData");
    glad_glBufferSubData = (decltype(glad_glBufferSubData))iOSMGLGetProcAddress("glBufferSubData");
    glad_glCallList = (decltype(glad_glCallList))iOSMGLGetProcAddress("glCallList");
    glad_glCallLists = (decltype(glad_glCallLists))iOSMGLGetProcAddress("glCallLists");
    glad_glCheckFramebufferStatus = (decltype(glad_glCheckFramebufferStatus))iOSMGLGetProcAddress("glCheckFramebufferStatus");
    glad_glClampColor = (decltype(glad_glClampColor))iOSMGLGetProcAddress("glClampColor");
    glad_glClear = (decltype(glad_glClear))iOSMGLGetProcAddress("glClear");
    glad_glClearAccum = (decltype(glad_glClearAccum))iOSMGLGetProcAddress("glClearAccum");
    glad_glClearBufferfi = (decltype(glad_glClearBufferfi))iOSMGLGetProcAddress("glClearBufferfi");
    glad_glClearBufferfv = (decltype(glad_glClearBufferfv))iOSMGLGetProcAddress("glClearBufferfv");
    glad_glClearBufferiv = (decltype(glad_glClearBufferiv))iOSMGLGetProcAddress("glClearBufferiv");
    glad_glClearBufferuiv = (decltype(glad_glClearBufferuiv))iOSMGLGetProcAddress("glClearBufferuiv");
    glad_glClearColor = (decltype(glad_glClearColor))iOSMGLGetProcAddress("glClearColor");
    glad_glClearDepth = (decltype(glad_glClearDepth))iOSMGLGetProcAddress("glClearDepth");
    glad_glClearDepthf = (decltype(glad_glClearDepthf))iOSMGLGetProcAddress("glClearDepthf");
    glad_glClearIndex = (decltype(glad_glClearIndex))iOSMGLGetProcAddress("glClearIndex");
    glad_glClearStencil = (decltype(glad_glClearStencil))iOSMGLGetProcAddress("glClearStencil");
    glad_glClientActiveTexture = (decltype(glad_glClientActiveTexture))iOSMGLGetProcAddress("glClientActiveTexture");
    glad_glClipPlane = (decltype(glad_glClipPlane))iOSMGLGetProcAddress("glClipPlane");
    glad_glColor3b = (decltype(glad_glColor3b))iOSMGLGetProcAddress("glColor3b");
    glad_glColor3bv = (decltype(glad_glColor3bv))iOSMGLGetProcAddress("glColor3bv");
    glad_glColor3d = (decltype(glad_glColor3d))iOSMGLGetProcAddress("glColor3d");
    glad_glColor3dv = (decltype(glad_glColor3dv))iOSMGLGetProcAddress("glColor3dv");
    glad_glColor3f = (decltype(glad_glColor3f))iOSMGLGetProcAddress("glColor3f");
    glad_glColor3fv = (decltype(glad_glColor3fv))iOSMGLGetProcAddress("glColor3fv");
    glad_glColor3i = (decltype(glad_glColor3i))iOSMGLGetProcAddress("glColor3i");
    glad_glColor3iv = (decltype(glad_glColor3iv))iOSMGLGetProcAddress("glColor3iv");
    glad_glColor3s = (decltype(glad_glColor3s))iOSMGLGetProcAddress("glColor3s");
    glad_glColor3sv = (decltype(glad_glColor3sv))iOSMGLGetProcAddress("glColor3sv");
    glad_glColor3ub = (decltype(glad_glColor3ub))iOSMGLGetProcAddress("glColor3ub");
    glad_glColor3ubv = (decltype(glad_glColor3ubv))iOSMGLGetProcAddress("glColor3ubv");
    glad_glColor3ui = (decltype(glad_glColor3ui))iOSMGLGetProcAddress("glColor3ui");
    glad_glColor3uiv = (decltype(glad_glColor3uiv))iOSMGLGetProcAddress("glColor3uiv");
    glad_glColor3us = (decltype(glad_glColor3us))iOSMGLGetProcAddress("glColor3us");
    glad_glColor3usv = (decltype(glad_glColor3usv))iOSMGLGetProcAddress("glColor3usv");
    glad_glColor4b = (decltype(glad_glColor4b))iOSMGLGetProcAddress("glColor4b");
    glad_glColor4bv = (decltype(glad_glColor4bv))iOSMGLGetProcAddress("glColor4bv");
    glad_glColor4d = (decltype(glad_glColor4d))iOSMGLGetProcAddress("glColor4d");
    glad_glColor4dv = (decltype(glad_glColor4dv))iOSMGLGetProcAddress("glColor4dv");
    glad_glColor4f = (decltype(glad_glColor4f))iOSMGLGetProcAddress("glColor4f");
    glad_glColor4fv = (decltype(glad_glColor4fv))iOSMGLGetProcAddress("glColor4fv");
    glad_glColor4i = (decltype(glad_glColor4i))iOSMGLGetProcAddress("glColor4i");
    glad_glColor4iv = (decltype(glad_glColor4iv))iOSMGLGetProcAddress("glColor4iv");
    glad_glColor4s = (decltype(glad_glColor4s))iOSMGLGetProcAddress("glColor4s");
    glad_glColor4sv = (decltype(glad_glColor4sv))iOSMGLGetProcAddress("glColor4sv");
    glad_glColor4ub = (decltype(glad_glColor4ub))iOSMGLGetProcAddress("glColor4ub");
    glad_glColor4ubv = (decltype(glad_glColor4ubv))iOSMGLGetProcAddress("glColor4ubv");
    glad_glColor4ui = (decltype(glad_glColor4ui))iOSMGLGetProcAddress("glColor4ui");
    glad_glColor4uiv = (decltype(glad_glColor4uiv))iOSMGLGetProcAddress("glColor4uiv");
    glad_glColor4us = (decltype(glad_glColor4us))iOSMGLGetProcAddress("glColor4us");
    glad_glColor4usv = (decltype(glad_glColor4usv))iOSMGLGetProcAddress("glColor4usv");
    glad_glColorMask = (decltype(glad_glColorMask))iOSMGLGetProcAddress("glColorMask");
    glad_glColorMaski = (decltype(glad_glColorMaski))iOSMGLGetProcAddress("glColorMaski");
    glad_glColorMaterial = (decltype(glad_glColorMaterial))iOSMGLGetProcAddress("glColorMaterial");
    glad_glColorPointer = (decltype(glad_glColorPointer))iOSMGLGetProcAddress("glColorPointer");
    glad_glCompileShader = (decltype(glad_glCompileShader))iOSMGLGetProcAddress("glCompileShader");
    glad_glCompressedTexImage1D = (decltype(glad_glCompressedTexImage1D))iOSMGLGetProcAddress("glCompressedTexImage1D");
    glad_glCompressedTexImage2D = (decltype(glad_glCompressedTexImage2D))iOSMGLGetProcAddress("glCompressedTexImage2D");
    glad_glCompressedTexImage3D = (decltype(glad_glCompressedTexImage3D))iOSMGLGetProcAddress("glCompressedTexImage3D");
    glad_glCompressedTexSubImage1D = (decltype(glad_glCompressedTexSubImage1D))iOSMGLGetProcAddress("glCompressedTexSubImage1D");
    glad_glCompressedTexSubImage2D = (decltype(glad_glCompressedTexSubImage2D))iOSMGLGetProcAddress("glCompressedTexSubImage2D");
    glad_glCompressedTexSubImage3D = (decltype(glad_glCompressedTexSubImage3D))iOSMGLGetProcAddress("glCompressedTexSubImage3D");
    glad_glCopyPixels = (decltype(glad_glCopyPixels))iOSMGLGetProcAddress("glCopyPixels");
    glad_glCopyTexImage1D = (decltype(glad_glCopyTexImage1D))iOSMGLGetProcAddress("glCopyTexImage1D");
    glad_glCopyTexImage2D = (decltype(glad_glCopyTexImage2D))iOSMGLGetProcAddress("glCopyTexImage2D");
    glad_glCopyTexSubImage1D = (decltype(glad_glCopyTexSubImage1D))iOSMGLGetProcAddress("glCopyTexSubImage1D");
    glad_glCopyTexSubImage2D = (decltype(glad_glCopyTexSubImage2D))iOSMGLGetProcAddress("glCopyTexSubImage2D");
    glad_glCopyTexSubImage3D = (decltype(glad_glCopyTexSubImage3D))iOSMGLGetProcAddress("glCopyTexSubImage3D");
    glad_glCreateProgram = (decltype(glad_glCreateProgram))iOSMGLGetProcAddress("glCreateProgram");
    glad_glCreateShader = (decltype(glad_glCreateShader))iOSMGLGetProcAddress("glCreateShader");
    glad_glCullFace = (decltype(glad_glCullFace))iOSMGLGetProcAddress("glCullFace");
    glad_glDebugMessageCallback = (decltype(glad_glDebugMessageCallback))iOSMGLGetProcAddress("glDebugMessageCallback");
    glad_glDebugMessageCallbackKHR = (decltype(glad_glDebugMessageCallbackKHR))iOSMGLGetProcAddress("glDebugMessageCallbackKHR");
    glad_glDebugMessageControl = (decltype(glad_glDebugMessageControl))iOSMGLGetProcAddress("glDebugMessageControl");
    glad_glDebugMessageControlKHR = (decltype(glad_glDebugMessageControlKHR))iOSMGLGetProcAddress("glDebugMessageControlKHR");
    glad_glDebugMessageInsert = (decltype(glad_glDebugMessageInsert))iOSMGLGetProcAddress("glDebugMessageInsert");
    glad_glDebugMessageInsertKHR = (decltype(glad_glDebugMessageInsertKHR))iOSMGLGetProcAddress("glDebugMessageInsertKHR");
    glad_glDeleteBuffers = (decltype(glad_glDeleteBuffers))iOSMGLGetProcAddress("glDeleteBuffers");
    glad_glDeleteFramebuffers = (decltype(glad_glDeleteFramebuffers))iOSMGLGetProcAddress("glDeleteFramebuffers");
    glad_glDeleteLists = (decltype(glad_glDeleteLists))iOSMGLGetProcAddress("glDeleteLists");
    glad_glDeleteProgram = (decltype(glad_glDeleteProgram))iOSMGLGetProcAddress("glDeleteProgram");
    glad_glDeleteQueries = (decltype(glad_glDeleteQueries))iOSMGLGetProcAddress("glDeleteQueries");
    glad_glDeleteRenderbuffers = (decltype(glad_glDeleteRenderbuffers))iOSMGLGetProcAddress("glDeleteRenderbuffers");
    glad_glDeleteShader = (decltype(glad_glDeleteShader))iOSMGLGetProcAddress("glDeleteShader");
    glad_glDeleteTextures = (decltype(glad_glDeleteTextures))iOSMGLGetProcAddress("glDeleteTextures");
    glad_glDeleteVertexArrays = (decltype(glad_glDeleteVertexArrays))iOSMGLGetProcAddress("glDeleteVertexArrays");
    glad_glDeleteVertexArraysOES = (decltype(glad_glDeleteVertexArraysOES))iOSMGLGetProcAddress("glDeleteVertexArraysOES");
    glad_glDepthFunc = (decltype(glad_glDepthFunc))iOSMGLGetProcAddress("glDepthFunc");
    glad_glDepthMask = (decltype(glad_glDepthMask))iOSMGLGetProcAddress("glDepthMask");
    glad_glDepthRange = (decltype(glad_glDepthRange))iOSMGLGetProcAddress("glDepthRange");
    glad_glDepthRangef = (decltype(glad_glDepthRangef))iOSMGLGetProcAddress("glDepthRangef");
    glad_glDetachShader = (decltype(glad_glDetachShader))iOSMGLGetProcAddress("glDetachShader");
    glad_glDisable = (decltype(glad_glDisable))iOSMGLGetProcAddress("glDisable");
    glad_glDisableClientState = (decltype(glad_glDisableClientState))iOSMGLGetProcAddress("glDisableClientState");
    glad_glDisableVertexAttribArray = (decltype(glad_glDisableVertexAttribArray))iOSMGLGetProcAddress("glDisableVertexAttribArray");
    glad_glDisablei = (decltype(glad_glDisablei))iOSMGLGetProcAddress("glDisablei");
    glad_glDrawArrays = (decltype(glad_glDrawArrays))iOSMGLGetProcAddress("glDrawArrays");
    glad_glDrawBuffer = (decltype(glad_glDrawBuffer))iOSMGLGetProcAddress("glDrawBuffer");
    glad_glDrawBuffers = (decltype(glad_glDrawBuffers))iOSMGLGetProcAddress("glDrawBuffers");
    glad_glDrawElements = (decltype(glad_glDrawElements))iOSMGLGetProcAddress("glDrawElements");
    glad_glDrawPixels = (decltype(glad_glDrawPixels))iOSMGLGetProcAddress("glDrawPixels");
    glad_glDrawRangeElements = (decltype(glad_glDrawRangeElements))iOSMGLGetProcAddress("glDrawRangeElements");
    glad_glEdgeFlag = (decltype(glad_glEdgeFlag))iOSMGLGetProcAddress("glEdgeFlag");
    glad_glEdgeFlagPointer = (decltype(glad_glEdgeFlagPointer))iOSMGLGetProcAddress("glEdgeFlagPointer");
    glad_glEdgeFlagv = (decltype(glad_glEdgeFlagv))iOSMGLGetProcAddress("glEdgeFlagv");
    glad_glEnable = (decltype(glad_glEnable))iOSMGLGetProcAddress("glEnable");
    glad_glEnableClientState = (decltype(glad_glEnableClientState))iOSMGLGetProcAddress("glEnableClientState");
    glad_glEnableVertexAttribArray = (decltype(glad_glEnableVertexAttribArray))iOSMGLGetProcAddress("glEnableVertexAttribArray");
    glad_glEnablei = (decltype(glad_glEnablei))iOSMGLGetProcAddress("glEnablei");
    glad_glEnd = (decltype(glad_glEnd))iOSMGLGetProcAddress("glEnd");
    glad_glEndConditionalRender = (decltype(glad_glEndConditionalRender))iOSMGLGetProcAddress("glEndConditionalRender");
    glad_glEndList = (decltype(glad_glEndList))iOSMGLGetProcAddress("glEndList");
    glad_glEndQuery = (decltype(glad_glEndQuery))iOSMGLGetProcAddress("glEndQuery");
    glad_glEndTransformFeedback = (decltype(glad_glEndTransformFeedback))iOSMGLGetProcAddress("glEndTransformFeedback");
    glad_glEvalCoord1d = (decltype(glad_glEvalCoord1d))iOSMGLGetProcAddress("glEvalCoord1d");
    glad_glEvalCoord1dv = (decltype(glad_glEvalCoord1dv))iOSMGLGetProcAddress("glEvalCoord1dv");
    glad_glEvalCoord1f = (decltype(glad_glEvalCoord1f))iOSMGLGetProcAddress("glEvalCoord1f");
    glad_glEvalCoord1fv = (decltype(glad_glEvalCoord1fv))iOSMGLGetProcAddress("glEvalCoord1fv");
    glad_glEvalCoord2d = (decltype(glad_glEvalCoord2d))iOSMGLGetProcAddress("glEvalCoord2d");
    glad_glEvalCoord2dv = (decltype(glad_glEvalCoord2dv))iOSMGLGetProcAddress("glEvalCoord2dv");
    glad_glEvalCoord2f = (decltype(glad_glEvalCoord2f))iOSMGLGetProcAddress("glEvalCoord2f");
    glad_glEvalCoord2fv = (decltype(glad_glEvalCoord2fv))iOSMGLGetProcAddress("glEvalCoord2fv");
    glad_glEvalMesh1 = (decltype(glad_glEvalMesh1))iOSMGLGetProcAddress("glEvalMesh1");
    glad_glEvalMesh2 = (decltype(glad_glEvalMesh2))iOSMGLGetProcAddress("glEvalMesh2");
    glad_glEvalPoint1 = (decltype(glad_glEvalPoint1))iOSMGLGetProcAddress("glEvalPoint1");
    glad_glEvalPoint2 = (decltype(glad_glEvalPoint2))iOSMGLGetProcAddress("glEvalPoint2");
    glad_glFeedbackBuffer = (decltype(glad_glFeedbackBuffer))iOSMGLGetProcAddress("glFeedbackBuffer");
    glad_glFinish = (decltype(glad_glFinish))iOSMGLGetProcAddress("glFinish");
    glad_glFlush = (decltype(glad_glFlush))iOSMGLGetProcAddress("glFlush");
    glad_glFlushMappedBufferRange = (decltype(glad_glFlushMappedBufferRange))iOSMGLGetProcAddress("glFlushMappedBufferRange");
    glad_glFogCoordPointer = (decltype(glad_glFogCoordPointer))iOSMGLGetProcAddress("glFogCoordPointer");
    glad_glFogCoordd = (decltype(glad_glFogCoordd))iOSMGLGetProcAddress("glFogCoordd");
    glad_glFogCoorddv = (decltype(glad_glFogCoorddv))iOSMGLGetProcAddress("glFogCoorddv");
    glad_glFogCoordf = (decltype(glad_glFogCoordf))iOSMGLGetProcAddress("glFogCoordf");
    glad_glFogCoordfv = (decltype(glad_glFogCoordfv))iOSMGLGetProcAddress("glFogCoordfv");
    glad_glFogf = (decltype(glad_glFogf))iOSMGLGetProcAddress("glFogf");
    glad_glFogfv = (decltype(glad_glFogfv))iOSMGLGetProcAddress("glFogfv");
    glad_glFogi = (decltype(glad_glFogi))iOSMGLGetProcAddress("glFogi");
    glad_glFogiv = (decltype(glad_glFogiv))iOSMGLGetProcAddress("glFogiv");
    glad_glFramebufferRenderbuffer = (decltype(glad_glFramebufferRenderbuffer))iOSMGLGetProcAddress("glFramebufferRenderbuffer");
    glad_glFramebufferTexture1D = (decltype(glad_glFramebufferTexture1D))iOSMGLGetProcAddress("glFramebufferTexture1D");
    glad_glFramebufferTexture2D = (decltype(glad_glFramebufferTexture2D))iOSMGLGetProcAddress("glFramebufferTexture2D");
    glad_glFramebufferTexture3D = (decltype(glad_glFramebufferTexture3D))iOSMGLGetProcAddress("glFramebufferTexture3D");
    glad_glFramebufferTextureLayer = (decltype(glad_glFramebufferTextureLayer))iOSMGLGetProcAddress("glFramebufferTextureLayer");
    glad_glFrontFace = (decltype(glad_glFrontFace))iOSMGLGetProcAddress("glFrontFace");
    glad_glFrustum = (decltype(glad_glFrustum))iOSMGLGetProcAddress("glFrustum");
    glad_glGenBuffers = (decltype(glad_glGenBuffers))iOSMGLGetProcAddress("glGenBuffers");
    glad_glGenFramebuffers = (decltype(glad_glGenFramebuffers))iOSMGLGetProcAddress("glGenFramebuffers");
    glad_glGenLists = (decltype(glad_glGenLists))iOSMGLGetProcAddress("glGenLists");
    glad_glGenQueries = (decltype(glad_glGenQueries))iOSMGLGetProcAddress("glGenQueries");
    glad_glGenRenderbuffers = (decltype(glad_glGenRenderbuffers))iOSMGLGetProcAddress("glGenRenderbuffers");
    glad_glGenTextures = (decltype(glad_glGenTextures))iOSMGLGetProcAddress("glGenTextures");
    glad_glGenVertexArrays = (decltype(glad_glGenVertexArrays))iOSMGLGetProcAddress("glGenVertexArrays");
    glad_glGenVertexArraysOES = (decltype(glad_glGenVertexArraysOES))iOSMGLGetProcAddress("glGenVertexArraysOES");
    glad_glGenerateMipmap = (decltype(glad_glGenerateMipmap))iOSMGLGetProcAddress("glGenerateMipmap");
    glad_glGetActiveAttrib = (decltype(glad_glGetActiveAttrib))iOSMGLGetProcAddress("glGetActiveAttrib");
    glad_glGetActiveUniform = (decltype(glad_glGetActiveUniform))iOSMGLGetProcAddress("glGetActiveUniform");
    glad_glGetAttachedShaders = (decltype(glad_glGetAttachedShaders))iOSMGLGetProcAddress("glGetAttachedShaders");
    glad_glGetAttribLocation = (decltype(glad_glGetAttribLocation))iOSMGLGetProcAddress("glGetAttribLocation");
    glad_glGetBooleanv = (decltype(glad_glGetBooleanv))iOSMGLGetProcAddress("glGetBooleanv");
    glad_glGetBufferParameteriv = (decltype(glad_glGetBufferParameteriv))iOSMGLGetProcAddress("glGetBufferParameteriv");
    glad_glGetBufferPointerv = (decltype(glad_glGetBufferPointerv))iOSMGLGetProcAddress("glGetBufferPointerv");
    glad_glGetBufferSubData = (decltype(glad_glGetBufferSubData))iOSMGLGetProcAddress("glGetBufferSubData");
    glad_glGetClipPlane = (decltype(glad_glGetClipPlane))iOSMGLGetProcAddress("glGetClipPlane");
    glad_glGetCompressedTexImage = (decltype(glad_glGetCompressedTexImage))iOSMGLGetProcAddress("glGetCompressedTexImage");
    glad_glGetDebugMessageLog = (decltype(glad_glGetDebugMessageLog))iOSMGLGetProcAddress("glGetDebugMessageLog");
    glad_glGetDebugMessageLogKHR = (decltype(glad_glGetDebugMessageLogKHR))iOSMGLGetProcAddress("glGetDebugMessageLogKHR");
    glad_glGetDoublev = (decltype(glad_glGetDoublev))iOSMGLGetProcAddress("glGetDoublev");
    glad_glGetError = (decltype(glad_glGetError))iOSMGLGetProcAddress("glGetError");
    glad_glGetFloatv = (decltype(glad_glGetFloatv))iOSMGLGetProcAddress("glGetFloatv");
    glad_glGetFragDataLocation = (decltype(glad_glGetFragDataLocation))iOSMGLGetProcAddress("glGetFragDataLocation");
    glad_glGetFramebufferAttachmentParameteriv = (decltype(glad_glGetFramebufferAttachmentParameteriv))iOSMGLGetProcAddress("glGetFramebufferAttachmentParameteriv");
    glad_glGetIntegerv = (decltype(glad_glGetIntegerv))iOSMGLGetProcAddress("glGetIntegerv");
    glad_glGetLightfv = (decltype(glad_glGetLightfv))iOSMGLGetProcAddress("glGetLightfv");
    glad_glGetLightiv = (decltype(glad_glGetLightiv))iOSMGLGetProcAddress("glGetLightiv");
    glad_glGetMapdv = (decltype(glad_glGetMapdv))iOSMGLGetProcAddress("glGetMapdv");
    glad_glGetMapfv = (decltype(glad_glGetMapfv))iOSMGLGetProcAddress("glGetMapfv");
    glad_glGetMapiv = (decltype(glad_glGetMapiv))iOSMGLGetProcAddress("glGetMapiv");
    glad_glGetMaterialfv = (decltype(glad_glGetMaterialfv))iOSMGLGetProcAddress("glGetMaterialfv");
    glad_glGetMaterialiv = (decltype(glad_glGetMaterialiv))iOSMGLGetProcAddress("glGetMaterialiv");
    glad_glGetObjectLabel = (decltype(glad_glGetObjectLabel))iOSMGLGetProcAddress("glGetObjectLabel");
    glad_glGetObjectLabelKHR = (decltype(glad_glGetObjectLabelKHR))iOSMGLGetProcAddress("glGetObjectLabelKHR");
    glad_glGetObjectPtrLabel = (decltype(glad_glGetObjectPtrLabel))iOSMGLGetProcAddress("glGetObjectPtrLabel");
    glad_glGetObjectPtrLabelKHR = (decltype(glad_glGetObjectPtrLabelKHR))iOSMGLGetProcAddress("glGetObjectPtrLabelKHR");
    glad_glGetPixelMapfv = (decltype(glad_glGetPixelMapfv))iOSMGLGetProcAddress("glGetPixelMapfv");
    glad_glGetPixelMapuiv = (decltype(glad_glGetPixelMapuiv))iOSMGLGetProcAddress("glGetPixelMapuiv");
    glad_glGetPixelMapusv = (decltype(glad_glGetPixelMapusv))iOSMGLGetProcAddress("glGetPixelMapusv");
    glad_glGetPointerv = (decltype(glad_glGetPointerv))iOSMGLGetProcAddress("glGetPointerv");
    glad_glGetPointervKHR = (decltype(glad_glGetPointervKHR))iOSMGLGetProcAddress("glGetPointervKHR");
    glad_glGetPolygonStipple = (decltype(glad_glGetPolygonStipple))iOSMGLGetProcAddress("glGetPolygonStipple");
    glad_glGetProgramInfoLog = (decltype(glad_glGetProgramInfoLog))iOSMGLGetProcAddress("glGetProgramInfoLog");
    glad_glGetProgramiv = (decltype(glad_glGetProgramiv))iOSMGLGetProcAddress("glGetProgramiv");
    glad_glGetQueryObjectiv = (decltype(glad_glGetQueryObjectiv))iOSMGLGetProcAddress("glGetQueryObjectiv");
    glad_glGetQueryObjectuiv = (decltype(glad_glGetQueryObjectuiv))iOSMGLGetProcAddress("glGetQueryObjectuiv");
    glad_glGetQueryiv = (decltype(glad_glGetQueryiv))iOSMGLGetProcAddress("glGetQueryiv");
    glad_glGetRenderbufferParameteriv = (decltype(glad_glGetRenderbufferParameteriv))iOSMGLGetProcAddress("glGetRenderbufferParameteriv");
    glad_glGetShaderInfoLog = (decltype(glad_glGetShaderInfoLog))iOSMGLGetProcAddress("glGetShaderInfoLog");
    glad_glGetShaderPrecisionFormat = (decltype(glad_glGetShaderPrecisionFormat))iOSMGLGetProcAddress("glGetShaderPrecisionFormat");
    glad_glGetShaderSource = (decltype(glad_glGetShaderSource))iOSMGLGetProcAddress("glGetShaderSource");
    glad_glGetShaderiv = (decltype(glad_glGetShaderiv))iOSMGLGetProcAddress("glGetShaderiv");
    glad_glGetString = (decltype(glad_glGetString))iOSMGLGetProcAddress("glGetString");
    glad_glGetStringi = (decltype(glad_glGetStringi))iOSMGLGetProcAddress("glGetStringi");
    glad_glGetTexEnvfv = (decltype(glad_glGetTexEnvfv))iOSMGLGetProcAddress("glGetTexEnvfv");
    glad_glGetTexEnviv = (decltype(glad_glGetTexEnviv))iOSMGLGetProcAddress("glGetTexEnviv");
    glad_glGetTexGendv = (decltype(glad_glGetTexGendv))iOSMGLGetProcAddress("glGetTexGendv");
    glad_glGetTexGenfv = (decltype(glad_glGetTexGenfv))iOSMGLGetProcAddress("glGetTexGenfv");
    glad_glGetTexGeniv = (decltype(glad_glGetTexGeniv))iOSMGLGetProcAddress("glGetTexGeniv");
    glad_glGetTexImage = (decltype(glad_glGetTexImage))iOSMGLGetProcAddress("glGetTexImage");
    glad_glGetTexLevelParameterfv = (decltype(glad_glGetTexLevelParameterfv))iOSMGLGetProcAddress("glGetTexLevelParameterfv");
    glad_glGetTexLevelParameteriv = (decltype(glad_glGetTexLevelParameteriv))iOSMGLGetProcAddress("glGetTexLevelParameteriv");
    glad_glGetTexParameterIiv = (decltype(glad_glGetTexParameterIiv))iOSMGLGetProcAddress("glGetTexParameterIiv");
    glad_glGetTexParameterIuiv = (decltype(glad_glGetTexParameterIuiv))iOSMGLGetProcAddress("glGetTexParameterIuiv");
    glad_glGetTexParameterfv = (decltype(glad_glGetTexParameterfv))iOSMGLGetProcAddress("glGetTexParameterfv");
    glad_glGetTexParameteriv = (decltype(glad_glGetTexParameteriv))iOSMGLGetProcAddress("glGetTexParameteriv");
    glad_glGetTransformFeedbackVarying = (decltype(glad_glGetTransformFeedbackVarying))iOSMGLGetProcAddress("glGetTransformFeedbackVarying");
    glad_glGetUniformLocation = (decltype(glad_glGetUniformLocation))iOSMGLGetProcAddress("glGetUniformLocation");
    glad_glGetUniformfv = (decltype(glad_glGetUniformfv))iOSMGLGetProcAddress("glGetUniformfv");
    glad_glGetUniformiv = (decltype(glad_glGetUniformiv))iOSMGLGetProcAddress("glGetUniformiv");
    glad_glGetUniformuiv = (decltype(glad_glGetUniformuiv))iOSMGLGetProcAddress("glGetUniformuiv");
    glad_glGetVertexAttribIiv = (decltype(glad_glGetVertexAttribIiv))iOSMGLGetProcAddress("glGetVertexAttribIiv");
    glad_glGetVertexAttribIuiv = (decltype(glad_glGetVertexAttribIuiv))iOSMGLGetProcAddress("glGetVertexAttribIuiv");
    glad_glGetVertexAttribPointerv = (decltype(glad_glGetVertexAttribPointerv))iOSMGLGetProcAddress("glGetVertexAttribPointerv");
    glad_glGetVertexAttribdv = (decltype(glad_glGetVertexAttribdv))iOSMGLGetProcAddress("glGetVertexAttribdv");
    glad_glGetVertexAttribfv = (decltype(glad_glGetVertexAttribfv))iOSMGLGetProcAddress("glGetVertexAttribfv");
    glad_glGetVertexAttribiv = (decltype(glad_glGetVertexAttribiv))iOSMGLGetProcAddress("glGetVertexAttribiv");
    glad_glHint = (decltype(glad_glHint))iOSMGLGetProcAddress("glHint");
    glad_glIndexMask = (decltype(glad_glIndexMask))iOSMGLGetProcAddress("glIndexMask");
    glad_glIndexPointer = (decltype(glad_glIndexPointer))iOSMGLGetProcAddress("glIndexPointer");
    glad_glIndexd = (decltype(glad_glIndexd))iOSMGLGetProcAddress("glIndexd");
    glad_glIndexdv = (decltype(glad_glIndexdv))iOSMGLGetProcAddress("glIndexdv");
    glad_glIndexf = (decltype(glad_glIndexf))iOSMGLGetProcAddress("glIndexf");
    glad_glIndexfv = (decltype(glad_glIndexfv))iOSMGLGetProcAddress("glIndexfv");
    glad_glIndexi = (decltype(glad_glIndexi))iOSMGLGetProcAddress("glIndexi");
    glad_glIndexiv = (decltype(glad_glIndexiv))iOSMGLGetProcAddress("glIndexiv");
    glad_glIndexs = (decltype(glad_glIndexs))iOSMGLGetProcAddress("glIndexs");
    glad_glIndexsv = (decltype(glad_glIndexsv))iOSMGLGetProcAddress("glIndexsv");
    glad_glIndexub = (decltype(glad_glIndexub))iOSMGLGetProcAddress("glIndexub");
    glad_glIndexubv = (decltype(glad_glIndexubv))iOSMGLGetProcAddress("glIndexubv");
    glad_glInitNames = (decltype(glad_glInitNames))iOSMGLGetProcAddress("glInitNames");
    glad_glInterleavedArrays = (decltype(glad_glInterleavedArrays))iOSMGLGetProcAddress("glInterleavedArrays");
    glad_glIsBuffer = (decltype(glad_glIsBuffer))iOSMGLGetProcAddress("glIsBuffer");
    glad_glIsEnabled = (decltype(glad_glIsEnabled))iOSMGLGetProcAddress("glIsEnabled");
    glad_glIsEnabledi = (decltype(glad_glIsEnabledi))iOSMGLGetProcAddress("glIsEnabledi");
    glad_glIsFramebuffer = (decltype(glad_glIsFramebuffer))iOSMGLGetProcAddress("glIsFramebuffer");
    glad_glIsList = (decltype(glad_glIsList))iOSMGLGetProcAddress("glIsList");
    glad_glIsProgram = (decltype(glad_glIsProgram))iOSMGLGetProcAddress("glIsProgram");
    glad_glIsQuery = (decltype(glad_glIsQuery))iOSMGLGetProcAddress("glIsQuery");
    glad_glIsRenderbuffer = (decltype(glad_glIsRenderbuffer))iOSMGLGetProcAddress("glIsRenderbuffer");
    glad_glIsShader = (decltype(glad_glIsShader))iOSMGLGetProcAddress("glIsShader");
    glad_glIsTexture = (decltype(glad_glIsTexture))iOSMGLGetProcAddress("glIsTexture");
    glad_glIsVertexArray = (decltype(glad_glIsVertexArray))iOSMGLGetProcAddress("glIsVertexArray");
    glad_glIsVertexArrayOES = (decltype(glad_glIsVertexArrayOES))iOSMGLGetProcAddress("glIsVertexArrayOES");
    glad_glLightModelf = (decltype(glad_glLightModelf))iOSMGLGetProcAddress("glLightModelf");
    glad_glLightModelfv = (decltype(glad_glLightModelfv))iOSMGLGetProcAddress("glLightModelfv");
    glad_glLightModeli = (decltype(glad_glLightModeli))iOSMGLGetProcAddress("glLightModeli");
    glad_glLightModeliv = (decltype(glad_glLightModeliv))iOSMGLGetProcAddress("glLightModeliv");
    glad_glLightf = (decltype(glad_glLightf))iOSMGLGetProcAddress("glLightf");
    glad_glLightfv = (decltype(glad_glLightfv))iOSMGLGetProcAddress("glLightfv");
    glad_glLighti = (decltype(glad_glLighti))iOSMGLGetProcAddress("glLighti");
    glad_glLightiv = (decltype(glad_glLightiv))iOSMGLGetProcAddress("glLightiv");
    glad_glLineStipple = (decltype(glad_glLineStipple))iOSMGLGetProcAddress("glLineStipple");
    glad_glLineWidth = (decltype(glad_glLineWidth))iOSMGLGetProcAddress("glLineWidth");
    glad_glLinkProgram = (decltype(glad_glLinkProgram))iOSMGLGetProcAddress("glLinkProgram");
    glad_glListBase = (decltype(glad_glListBase))iOSMGLGetProcAddress("glListBase");
    glad_glLoadIdentity = (decltype(glad_glLoadIdentity))iOSMGLGetProcAddress("glLoadIdentity");
    glad_glLoadMatrixd = (decltype(glad_glLoadMatrixd))iOSMGLGetProcAddress("glLoadMatrixd");
    glad_glLoadMatrixf = (decltype(glad_glLoadMatrixf))iOSMGLGetProcAddress("glLoadMatrixf");
    glad_glLoadName = (decltype(glad_glLoadName))iOSMGLGetProcAddress("glLoadName");
    glad_glLoadTransposeMatrixd = (decltype(glad_glLoadTransposeMatrixd))iOSMGLGetProcAddress("glLoadTransposeMatrixd");
    glad_glLoadTransposeMatrixf = (decltype(glad_glLoadTransposeMatrixf))iOSMGLGetProcAddress("glLoadTransposeMatrixf");
    glad_glLogicOp = (decltype(glad_glLogicOp))iOSMGLGetProcAddress("glLogicOp");
    glad_glMap1d = (decltype(glad_glMap1d))iOSMGLGetProcAddress("glMap1d");
    glad_glMap1f = (decltype(glad_glMap1f))iOSMGLGetProcAddress("glMap1f");
    glad_glMap2d = (decltype(glad_glMap2d))iOSMGLGetProcAddress("glMap2d");
    glad_glMap2f = (decltype(glad_glMap2f))iOSMGLGetProcAddress("glMap2f");
    glad_glMapBuffer = (decltype(glad_glMapBuffer))iOSMGLGetProcAddress("glMapBuffer");
    glad_glMapBufferRange = (decltype(glad_glMapBufferRange))iOSMGLGetProcAddress("glMapBufferRange");
    glad_glMapGrid1d = (decltype(glad_glMapGrid1d))iOSMGLGetProcAddress("glMapGrid1d");
    glad_glMapGrid1f = (decltype(glad_glMapGrid1f))iOSMGLGetProcAddress("glMapGrid1f");
    glad_glMapGrid2d = (decltype(glad_glMapGrid2d))iOSMGLGetProcAddress("glMapGrid2d");
    glad_glMapGrid2f = (decltype(glad_glMapGrid2f))iOSMGLGetProcAddress("glMapGrid2f");
    glad_glMaterialf = (decltype(glad_glMaterialf))iOSMGLGetProcAddress("glMaterialf");
    glad_glMaterialfv = (decltype(glad_glMaterialfv))iOSMGLGetProcAddress("glMaterialfv");
    glad_glMateriali = (decltype(glad_glMateriali))iOSMGLGetProcAddress("glMateriali");
    glad_glMaterialiv = (decltype(glad_glMaterialiv))iOSMGLGetProcAddress("glMaterialiv");
    glad_glMatrixMode = (decltype(glad_glMatrixMode))iOSMGLGetProcAddress("glMatrixMode");
    glad_glMultMatrixd = (decltype(glad_glMultMatrixd))iOSMGLGetProcAddress("glMultMatrixd");
    glad_glMultMatrixf = (decltype(glad_glMultMatrixf))iOSMGLGetProcAddress("glMultMatrixf");
    glad_glMultTransposeMatrixd = (decltype(glad_glMultTransposeMatrixd))iOSMGLGetProcAddress("glMultTransposeMatrixd");
    glad_glMultTransposeMatrixf = (decltype(glad_glMultTransposeMatrixf))iOSMGLGetProcAddress("glMultTransposeMatrixf");
    glad_glMultiDrawArrays = (decltype(glad_glMultiDrawArrays))iOSMGLGetProcAddress("glMultiDrawArrays");
    glad_glMultiDrawElements = (decltype(glad_glMultiDrawElements))iOSMGLGetProcAddress("glMultiDrawElements");
    glad_glMultiTexCoord1d = (decltype(glad_glMultiTexCoord1d))iOSMGLGetProcAddress("glMultiTexCoord1d");
    glad_glMultiTexCoord1dv = (decltype(glad_glMultiTexCoord1dv))iOSMGLGetProcAddress("glMultiTexCoord1dv");
    glad_glMultiTexCoord1f = (decltype(glad_glMultiTexCoord1f))iOSMGLGetProcAddress("glMultiTexCoord1f");
    glad_glMultiTexCoord1fv = (decltype(glad_glMultiTexCoord1fv))iOSMGLGetProcAddress("glMultiTexCoord1fv");
    glad_glMultiTexCoord1i = (decltype(glad_glMultiTexCoord1i))iOSMGLGetProcAddress("glMultiTexCoord1i");
    glad_glMultiTexCoord1iv = (decltype(glad_glMultiTexCoord1iv))iOSMGLGetProcAddress("glMultiTexCoord1iv");
    glad_glMultiTexCoord1s = (decltype(glad_glMultiTexCoord1s))iOSMGLGetProcAddress("glMultiTexCoord1s");
    glad_glMultiTexCoord1sv = (decltype(glad_glMultiTexCoord1sv))iOSMGLGetProcAddress("glMultiTexCoord1sv");
    glad_glMultiTexCoord2d = (decltype(glad_glMultiTexCoord2d))iOSMGLGetProcAddress("glMultiTexCoord2d");
    glad_glMultiTexCoord2dv = (decltype(glad_glMultiTexCoord2dv))iOSMGLGetProcAddress("glMultiTexCoord2dv");
    glad_glMultiTexCoord2f = (decltype(glad_glMultiTexCoord2f))iOSMGLGetProcAddress("glMultiTexCoord2f");
    glad_glMultiTexCoord2fv = (decltype(glad_glMultiTexCoord2fv))iOSMGLGetProcAddress("glMultiTexCoord2fv");
    glad_glMultiTexCoord2i = (decltype(glad_glMultiTexCoord2i))iOSMGLGetProcAddress("glMultiTexCoord2i");
    glad_glMultiTexCoord2iv = (decltype(glad_glMultiTexCoord2iv))iOSMGLGetProcAddress("glMultiTexCoord2iv");
    glad_glMultiTexCoord2s = (decltype(glad_glMultiTexCoord2s))iOSMGLGetProcAddress("glMultiTexCoord2s");
    glad_glMultiTexCoord2sv = (decltype(glad_glMultiTexCoord2sv))iOSMGLGetProcAddress("glMultiTexCoord2sv");
    glad_glMultiTexCoord3d = (decltype(glad_glMultiTexCoord3d))iOSMGLGetProcAddress("glMultiTexCoord3d");
    glad_glMultiTexCoord3dv = (decltype(glad_glMultiTexCoord3dv))iOSMGLGetProcAddress("glMultiTexCoord3dv");
    glad_glMultiTexCoord3f = (decltype(glad_glMultiTexCoord3f))iOSMGLGetProcAddress("glMultiTexCoord3f");
    glad_glMultiTexCoord3fv = (decltype(glad_glMultiTexCoord3fv))iOSMGLGetProcAddress("glMultiTexCoord3fv");
    glad_glMultiTexCoord3i = (decltype(glad_glMultiTexCoord3i))iOSMGLGetProcAddress("glMultiTexCoord3i");
    glad_glMultiTexCoord3iv = (decltype(glad_glMultiTexCoord3iv))iOSMGLGetProcAddress("glMultiTexCoord3iv");
    glad_glMultiTexCoord3s = (decltype(glad_glMultiTexCoord3s))iOSMGLGetProcAddress("glMultiTexCoord3s");
    glad_glMultiTexCoord3sv = (decltype(glad_glMultiTexCoord3sv))iOSMGLGetProcAddress("glMultiTexCoord3sv");
    glad_glMultiTexCoord4d = (decltype(glad_glMultiTexCoord4d))iOSMGLGetProcAddress("glMultiTexCoord4d");
    glad_glMultiTexCoord4dv = (decltype(glad_glMultiTexCoord4dv))iOSMGLGetProcAddress("glMultiTexCoord4dv");
    glad_glMultiTexCoord4f = (decltype(glad_glMultiTexCoord4f))iOSMGLGetProcAddress("glMultiTexCoord4f");
    glad_glMultiTexCoord4fv = (decltype(glad_glMultiTexCoord4fv))iOSMGLGetProcAddress("glMultiTexCoord4fv");
    glad_glMultiTexCoord4i = (decltype(glad_glMultiTexCoord4i))iOSMGLGetProcAddress("glMultiTexCoord4i");
    glad_glMultiTexCoord4iv = (decltype(glad_glMultiTexCoord4iv))iOSMGLGetProcAddress("glMultiTexCoord4iv");
    glad_glMultiTexCoord4s = (decltype(glad_glMultiTexCoord4s))iOSMGLGetProcAddress("glMultiTexCoord4s");
    glad_glMultiTexCoord4sv = (decltype(glad_glMultiTexCoord4sv))iOSMGLGetProcAddress("glMultiTexCoord4sv");
    glad_glNewList = (decltype(glad_glNewList))iOSMGLGetProcAddress("glNewList");
    glad_glNormal3b = (decltype(glad_glNormal3b))iOSMGLGetProcAddress("glNormal3b");
    glad_glNormal3bv = (decltype(glad_glNormal3bv))iOSMGLGetProcAddress("glNormal3bv");
    glad_glNormal3d = (decltype(glad_glNormal3d))iOSMGLGetProcAddress("glNormal3d");
    glad_glNormal3dv = (decltype(glad_glNormal3dv))iOSMGLGetProcAddress("glNormal3dv");
    glad_glNormal3f = (decltype(glad_glNormal3f))iOSMGLGetProcAddress("glNormal3f");
    glad_glNormal3fv = (decltype(glad_glNormal3fv))iOSMGLGetProcAddress("glNormal3fv");
    glad_glNormal3i = (decltype(glad_glNormal3i))iOSMGLGetProcAddress("glNormal3i");
    glad_glNormal3iv = (decltype(glad_glNormal3iv))iOSMGLGetProcAddress("glNormal3iv");
    glad_glNormal3s = (decltype(glad_glNormal3s))iOSMGLGetProcAddress("glNormal3s");
    glad_glNormal3sv = (decltype(glad_glNormal3sv))iOSMGLGetProcAddress("glNormal3sv");
    glad_glNormalPointer = (decltype(glad_glNormalPointer))iOSMGLGetProcAddress("glNormalPointer");
    glad_glObjectLabel = (decltype(glad_glObjectLabel))iOSMGLGetProcAddress("glObjectLabel");
    glad_glObjectLabelKHR = (decltype(glad_glObjectLabelKHR))iOSMGLGetProcAddress("glObjectLabelKHR");
    glad_glObjectPtrLabel = (decltype(glad_glObjectPtrLabel))iOSMGLGetProcAddress("glObjectPtrLabel");
    glad_glObjectPtrLabelKHR = (decltype(glad_glObjectPtrLabelKHR))iOSMGLGetProcAddress("glObjectPtrLabelKHR");
    glad_glOrtho = (decltype(glad_glOrtho))iOSMGLGetProcAddress("glOrtho");
    glad_glPassThrough = (decltype(glad_glPassThrough))iOSMGLGetProcAddress("glPassThrough");
    glad_glPixelMapfv = (decltype(glad_glPixelMapfv))iOSMGLGetProcAddress("glPixelMapfv");
    glad_glPixelMapuiv = (decltype(glad_glPixelMapuiv))iOSMGLGetProcAddress("glPixelMapuiv");
    glad_glPixelMapusv = (decltype(glad_glPixelMapusv))iOSMGLGetProcAddress("glPixelMapusv");
    glad_glPixelStoref = (decltype(glad_glPixelStoref))iOSMGLGetProcAddress("glPixelStoref");
    glad_glPixelStorei = (decltype(glad_glPixelStorei))iOSMGLGetProcAddress("glPixelStorei");
    glad_glPixelTransferf = (decltype(glad_glPixelTransferf))iOSMGLGetProcAddress("glPixelTransferf");
    glad_glPixelTransferi = (decltype(glad_glPixelTransferi))iOSMGLGetProcAddress("glPixelTransferi");
    glad_glPixelZoom = (decltype(glad_glPixelZoom))iOSMGLGetProcAddress("glPixelZoom");
    glad_glPointParameterf = (decltype(glad_glPointParameterf))iOSMGLGetProcAddress("glPointParameterf");
    glad_glPointParameterfv = (decltype(glad_glPointParameterfv))iOSMGLGetProcAddress("glPointParameterfv");
    glad_glPointParameteri = (decltype(glad_glPointParameteri))iOSMGLGetProcAddress("glPointParameteri");
    glad_glPointParameteriv = (decltype(glad_glPointParameteriv))iOSMGLGetProcAddress("glPointParameteriv");
    glad_glPointSize = (decltype(glad_glPointSize))iOSMGLGetProcAddress("glPointSize");
    glad_glPolygonMode = (decltype(glad_glPolygonMode))iOSMGLGetProcAddress("glPolygonMode");
    glad_glPolygonOffset = (decltype(glad_glPolygonOffset))iOSMGLGetProcAddress("glPolygonOffset");
    glad_glPolygonStipple = (decltype(glad_glPolygonStipple))iOSMGLGetProcAddress("glPolygonStipple");
    glad_glPopAttrib = (decltype(glad_glPopAttrib))iOSMGLGetProcAddress("glPopAttrib");
    glad_glPopClientAttrib = (decltype(glad_glPopClientAttrib))iOSMGLGetProcAddress("glPopClientAttrib");
    glad_glPopDebugGroup = (decltype(glad_glPopDebugGroup))iOSMGLGetProcAddress("glPopDebugGroup");
    glad_glPopDebugGroupKHR = (decltype(glad_glPopDebugGroupKHR))iOSMGLGetProcAddress("glPopDebugGroupKHR");
    glad_glPopMatrix = (decltype(glad_glPopMatrix))iOSMGLGetProcAddress("glPopMatrix");
    glad_glPopName = (decltype(glad_glPopName))iOSMGLGetProcAddress("glPopName");
    glad_glPrioritizeTextures = (decltype(glad_glPrioritizeTextures))iOSMGLGetProcAddress("glPrioritizeTextures");
    glad_glPushAttrib = (decltype(glad_glPushAttrib))iOSMGLGetProcAddress("glPushAttrib");
    glad_glPushClientAttrib = (decltype(glad_glPushClientAttrib))iOSMGLGetProcAddress("glPushClientAttrib");
    glad_glPushDebugGroup = (decltype(glad_glPushDebugGroup))iOSMGLGetProcAddress("glPushDebugGroup");
    glad_glPushDebugGroupKHR = (decltype(glad_glPushDebugGroupKHR))iOSMGLGetProcAddress("glPushDebugGroupKHR");
    glad_glPushMatrix = (decltype(glad_glPushMatrix))iOSMGLGetProcAddress("glPushMatrix");
    glad_glPushName = (decltype(glad_glPushName))iOSMGLGetProcAddress("glPushName");
    glad_glRasterPos2d = (decltype(glad_glRasterPos2d))iOSMGLGetProcAddress("glRasterPos2d");
    glad_glRasterPos2dv = (decltype(glad_glRasterPos2dv))iOSMGLGetProcAddress("glRasterPos2dv");
    glad_glRasterPos2f = (decltype(glad_glRasterPos2f))iOSMGLGetProcAddress("glRasterPos2f");
    glad_glRasterPos2fv = (decltype(glad_glRasterPos2fv))iOSMGLGetProcAddress("glRasterPos2fv");
    glad_glRasterPos2i = (decltype(glad_glRasterPos2i))iOSMGLGetProcAddress("glRasterPos2i");
    glad_glRasterPos2iv = (decltype(glad_glRasterPos2iv))iOSMGLGetProcAddress("glRasterPos2iv");
    glad_glRasterPos2s = (decltype(glad_glRasterPos2s))iOSMGLGetProcAddress("glRasterPos2s");
    glad_glRasterPos2sv = (decltype(glad_glRasterPos2sv))iOSMGLGetProcAddress("glRasterPos2sv");
    glad_glRasterPos3d = (decltype(glad_glRasterPos3d))iOSMGLGetProcAddress("glRasterPos3d");
    glad_glRasterPos3dv = (decltype(glad_glRasterPos3dv))iOSMGLGetProcAddress("glRasterPos3dv");
    glad_glRasterPos3f = (decltype(glad_glRasterPos3f))iOSMGLGetProcAddress("glRasterPos3f");
    glad_glRasterPos3fv = (decltype(glad_glRasterPos3fv))iOSMGLGetProcAddress("glRasterPos3fv");
    glad_glRasterPos3i = (decltype(glad_glRasterPos3i))iOSMGLGetProcAddress("glRasterPos3i");
    glad_glRasterPos3iv = (decltype(glad_glRasterPos3iv))iOSMGLGetProcAddress("glRasterPos3iv");
    glad_glRasterPos3s = (decltype(glad_glRasterPos3s))iOSMGLGetProcAddress("glRasterPos3s");
    glad_glRasterPos3sv = (decltype(glad_glRasterPos3sv))iOSMGLGetProcAddress("glRasterPos3sv");
    glad_glRasterPos4d = (decltype(glad_glRasterPos4d))iOSMGLGetProcAddress("glRasterPos4d");
    glad_glRasterPos4dv = (decltype(glad_glRasterPos4dv))iOSMGLGetProcAddress("glRasterPos4dv");
    glad_glRasterPos4f = (decltype(glad_glRasterPos4f))iOSMGLGetProcAddress("glRasterPos4f");
    glad_glRasterPos4fv = (decltype(glad_glRasterPos4fv))iOSMGLGetProcAddress("glRasterPos4fv");
    glad_glRasterPos4i = (decltype(glad_glRasterPos4i))iOSMGLGetProcAddress("glRasterPos4i");
    glad_glRasterPos4iv = (decltype(glad_glRasterPos4iv))iOSMGLGetProcAddress("glRasterPos4iv");
    glad_glRasterPos4s = (decltype(glad_glRasterPos4s))iOSMGLGetProcAddress("glRasterPos4s");
    glad_glRasterPos4sv = (decltype(glad_glRasterPos4sv))iOSMGLGetProcAddress("glRasterPos4sv");
    glad_glReadBuffer = (decltype(glad_glReadBuffer))iOSMGLGetProcAddress("glReadBuffer");
    glad_glReadPixels = (decltype(glad_glReadPixels))iOSMGLGetProcAddress("glReadPixels");
    glad_glRectd = (decltype(glad_glRectd))iOSMGLGetProcAddress("glRectd");
    glad_glRectdv = (decltype(glad_glRectdv))iOSMGLGetProcAddress("glRectdv");
    glad_glRectf = (decltype(glad_glRectf))iOSMGLGetProcAddress("glRectf");
    glad_glRectfv = (decltype(glad_glRectfv))iOSMGLGetProcAddress("glRectfv");
    glad_glRecti = (decltype(glad_glRecti))iOSMGLGetProcAddress("glRecti");
    glad_glRectiv = (decltype(glad_glRectiv))iOSMGLGetProcAddress("glRectiv");
    glad_glRects = (decltype(glad_glRects))iOSMGLGetProcAddress("glRects");
    glad_glRectsv = (decltype(glad_glRectsv))iOSMGLGetProcAddress("glRectsv");
    glad_glReleaseShaderCompiler = (decltype(glad_glReleaseShaderCompiler))iOSMGLGetProcAddress("glReleaseShaderCompiler");
    glad_glRenderMode = (decltype(glad_glRenderMode))iOSMGLGetProcAddress("glRenderMode");
    glad_glRenderbufferStorage = (decltype(glad_glRenderbufferStorage))iOSMGLGetProcAddress("glRenderbufferStorage");
    glad_glRenderbufferStorageMultisample = (decltype(glad_glRenderbufferStorageMultisample))iOSMGLGetProcAddress("glRenderbufferStorageMultisample");
    glad_glRotated = (decltype(glad_glRotated))iOSMGLGetProcAddress("glRotated");
    glad_glRotatef = (decltype(glad_glRotatef))iOSMGLGetProcAddress("glRotatef");
    glad_glSampleCoverage = (decltype(glad_glSampleCoverage))iOSMGLGetProcAddress("glSampleCoverage");
    glad_glScaled = (decltype(glad_glScaled))iOSMGLGetProcAddress("glScaled");
    glad_glScalef = (decltype(glad_glScalef))iOSMGLGetProcAddress("glScalef");
    glad_glScissor = (decltype(glad_glScissor))iOSMGLGetProcAddress("glScissor");
    glad_glSecondaryColor3b = (decltype(glad_glSecondaryColor3b))iOSMGLGetProcAddress("glSecondaryColor3b");
    glad_glSecondaryColor3bv = (decltype(glad_glSecondaryColor3bv))iOSMGLGetProcAddress("glSecondaryColor3bv");
    glad_glSecondaryColor3d = (decltype(glad_glSecondaryColor3d))iOSMGLGetProcAddress("glSecondaryColor3d");
    glad_glSecondaryColor3dv = (decltype(glad_glSecondaryColor3dv))iOSMGLGetProcAddress("glSecondaryColor3dv");
    glad_glSecondaryColor3f = (decltype(glad_glSecondaryColor3f))iOSMGLGetProcAddress("glSecondaryColor3f");
    glad_glSecondaryColor3fv = (decltype(glad_glSecondaryColor3fv))iOSMGLGetProcAddress("glSecondaryColor3fv");
    glad_glSecondaryColor3i = (decltype(glad_glSecondaryColor3i))iOSMGLGetProcAddress("glSecondaryColor3i");
    glad_glSecondaryColor3iv = (decltype(glad_glSecondaryColor3iv))iOSMGLGetProcAddress("glSecondaryColor3iv");
    glad_glSecondaryColor3s = (decltype(glad_glSecondaryColor3s))iOSMGLGetProcAddress("glSecondaryColor3s");
    glad_glSecondaryColor3sv = (decltype(glad_glSecondaryColor3sv))iOSMGLGetProcAddress("glSecondaryColor3sv");
    glad_glSecondaryColor3ub = (decltype(glad_glSecondaryColor3ub))iOSMGLGetProcAddress("glSecondaryColor3ub");
    glad_glSecondaryColor3ubv = (decltype(glad_glSecondaryColor3ubv))iOSMGLGetProcAddress("glSecondaryColor3ubv");
    glad_glSecondaryColor3ui = (decltype(glad_glSecondaryColor3ui))iOSMGLGetProcAddress("glSecondaryColor3ui");
    glad_glSecondaryColor3uiv = (decltype(glad_glSecondaryColor3uiv))iOSMGLGetProcAddress("glSecondaryColor3uiv");
    glad_glSecondaryColor3us = (decltype(glad_glSecondaryColor3us))iOSMGLGetProcAddress("glSecondaryColor3us");
    glad_glSecondaryColor3usv = (decltype(glad_glSecondaryColor3usv))iOSMGLGetProcAddress("glSecondaryColor3usv");
    glad_glSecondaryColorPointer = (decltype(glad_glSecondaryColorPointer))iOSMGLGetProcAddress("glSecondaryColorPointer");
    glad_glSelectBuffer = (decltype(glad_glSelectBuffer))iOSMGLGetProcAddress("glSelectBuffer");
    glad_glShadeModel = (decltype(glad_glShadeModel))iOSMGLGetProcAddress("glShadeModel");
    glad_glShaderBinary = (decltype(glad_glShaderBinary))iOSMGLGetProcAddress("glShaderBinary");
    glad_glShaderSource = (decltype(glad_glShaderSource))iOSMGLGetProcAddress("glShaderSource");
    glad_glStencilFunc = (decltype(glad_glStencilFunc))iOSMGLGetProcAddress("glStencilFunc");
    glad_glStencilFuncSeparate = (decltype(glad_glStencilFuncSeparate))iOSMGLGetProcAddress("glStencilFuncSeparate");
    glad_glStencilMask = (decltype(glad_glStencilMask))iOSMGLGetProcAddress("glStencilMask");
    glad_glStencilMaskSeparate = (decltype(glad_glStencilMaskSeparate))iOSMGLGetProcAddress("glStencilMaskSeparate");
    glad_glStencilOp = (decltype(glad_glStencilOp))iOSMGLGetProcAddress("glStencilOp");
    glad_glStencilOpSeparate = (decltype(glad_glStencilOpSeparate))iOSMGLGetProcAddress("glStencilOpSeparate");
    glad_glTexCoord1d = (decltype(glad_glTexCoord1d))iOSMGLGetProcAddress("glTexCoord1d");
    glad_glTexCoord1dv = (decltype(glad_glTexCoord1dv))iOSMGLGetProcAddress("glTexCoord1dv");
    glad_glTexCoord1f = (decltype(glad_glTexCoord1f))iOSMGLGetProcAddress("glTexCoord1f");
    glad_glTexCoord1fv = (decltype(glad_glTexCoord1fv))iOSMGLGetProcAddress("glTexCoord1fv");
    glad_glTexCoord1i = (decltype(glad_glTexCoord1i))iOSMGLGetProcAddress("glTexCoord1i");
    glad_glTexCoord1iv = (decltype(glad_glTexCoord1iv))iOSMGLGetProcAddress("glTexCoord1iv");
    glad_glTexCoord1s = (decltype(glad_glTexCoord1s))iOSMGLGetProcAddress("glTexCoord1s");
    glad_glTexCoord1sv = (decltype(glad_glTexCoord1sv))iOSMGLGetProcAddress("glTexCoord1sv");
    glad_glTexCoord2d = (decltype(glad_glTexCoord2d))iOSMGLGetProcAddress("glTexCoord2d");
    glad_glTexCoord2dv = (decltype(glad_glTexCoord2dv))iOSMGLGetProcAddress("glTexCoord2dv");
    glad_glTexCoord2f = (decltype(glad_glTexCoord2f))iOSMGLGetProcAddress("glTexCoord2f");
    glad_glTexCoord2fv = (decltype(glad_glTexCoord2fv))iOSMGLGetProcAddress("glTexCoord2fv");
    glad_glTexCoord2i = (decltype(glad_glTexCoord2i))iOSMGLGetProcAddress("glTexCoord2i");
    glad_glTexCoord2iv = (decltype(glad_glTexCoord2iv))iOSMGLGetProcAddress("glTexCoord2iv");
    glad_glTexCoord2s = (decltype(glad_glTexCoord2s))iOSMGLGetProcAddress("glTexCoord2s");
    glad_glTexCoord2sv = (decltype(glad_glTexCoord2sv))iOSMGLGetProcAddress("glTexCoord2sv");
    glad_glTexCoord3d = (decltype(glad_glTexCoord3d))iOSMGLGetProcAddress("glTexCoord3d");
    glad_glTexCoord3dv = (decltype(glad_glTexCoord3dv))iOSMGLGetProcAddress("glTexCoord3dv");
    glad_glTexCoord3f = (decltype(glad_glTexCoord3f))iOSMGLGetProcAddress("glTexCoord3f");
    glad_glTexCoord3fv = (decltype(glad_glTexCoord3fv))iOSMGLGetProcAddress("glTexCoord3fv");
    glad_glTexCoord3i = (decltype(glad_glTexCoord3i))iOSMGLGetProcAddress("glTexCoord3i");
    glad_glTexCoord3iv = (decltype(glad_glTexCoord3iv))iOSMGLGetProcAddress("glTexCoord3iv");
    glad_glTexCoord3s = (decltype(glad_glTexCoord3s))iOSMGLGetProcAddress("glTexCoord3s");
    glad_glTexCoord3sv = (decltype(glad_glTexCoord3sv))iOSMGLGetProcAddress("glTexCoord3sv");
    glad_glTexCoord4d = (decltype(glad_glTexCoord4d))iOSMGLGetProcAddress("glTexCoord4d");
    glad_glTexCoord4dv = (decltype(glad_glTexCoord4dv))iOSMGLGetProcAddress("glTexCoord4dv");
    glad_glTexCoord4f = (decltype(glad_glTexCoord4f))iOSMGLGetProcAddress("glTexCoord4f");
    glad_glTexCoord4fv = (decltype(glad_glTexCoord4fv))iOSMGLGetProcAddress("glTexCoord4fv");
    glad_glTexCoord4i = (decltype(glad_glTexCoord4i))iOSMGLGetProcAddress("glTexCoord4i");
    glad_glTexCoord4iv = (decltype(glad_glTexCoord4iv))iOSMGLGetProcAddress("glTexCoord4iv");
    glad_glTexCoord4s = (decltype(glad_glTexCoord4s))iOSMGLGetProcAddress("glTexCoord4s");
    glad_glTexCoord4sv = (decltype(glad_glTexCoord4sv))iOSMGLGetProcAddress("glTexCoord4sv");
    glad_glTexCoordPointer = (decltype(glad_glTexCoordPointer))iOSMGLGetProcAddress("glTexCoordPointer");
    glad_glTexEnvf = (decltype(glad_glTexEnvf))iOSMGLGetProcAddress("glTexEnvf");
    glad_glTexEnvfv = (decltype(glad_glTexEnvfv))iOSMGLGetProcAddress("glTexEnvfv");
    glad_glTexEnvi = (decltype(glad_glTexEnvi))iOSMGLGetProcAddress("glTexEnvi");
    glad_glTexEnviv = (decltype(glad_glTexEnviv))iOSMGLGetProcAddress("glTexEnviv");
    glad_glTexGend = (decltype(glad_glTexGend))iOSMGLGetProcAddress("glTexGend");
    glad_glTexGendv = (decltype(glad_glTexGendv))iOSMGLGetProcAddress("glTexGendv");
    glad_glTexGenf = (decltype(glad_glTexGenf))iOSMGLGetProcAddress("glTexGenf");
    glad_glTexGenfv = (decltype(glad_glTexGenfv))iOSMGLGetProcAddress("glTexGenfv");
    glad_glTexGeni = (decltype(glad_glTexGeni))iOSMGLGetProcAddress("glTexGeni");
    glad_glTexGeniv = (decltype(glad_glTexGeniv))iOSMGLGetProcAddress("glTexGeniv");
    glad_glTexImage1D = (decltype(glad_glTexImage1D))iOSMGLGetProcAddress("glTexImage1D");
    glad_glTexImage2D = (decltype(glad_glTexImage2D))iOSMGLGetProcAddress("glTexImage2D");
    glad_glTexImage3D = (decltype(glad_glTexImage3D))iOSMGLGetProcAddress("glTexImage3D");
    glad_glTexParameterIiv = (decltype(glad_glTexParameterIiv))iOSMGLGetProcAddress("glTexParameterIiv");
    glad_glTexParameterIuiv = (decltype(glad_glTexParameterIuiv))iOSMGLGetProcAddress("glTexParameterIuiv");
    glad_glTexParameterf = (decltype(glad_glTexParameterf))iOSMGLGetProcAddress("glTexParameterf");
    glad_glTexParameterfv = (decltype(glad_glTexParameterfv))iOSMGLGetProcAddress("glTexParameterfv");
    glad_glTexParameteri = (decltype(glad_glTexParameteri))iOSMGLGetProcAddress("glTexParameteri");
    glad_glTexParameteriv = (decltype(glad_glTexParameteriv))iOSMGLGetProcAddress("glTexParameteriv");
    glad_glTexSubImage1D = (decltype(glad_glTexSubImage1D))iOSMGLGetProcAddress("glTexSubImage1D");
    glad_glTexSubImage2D = (decltype(glad_glTexSubImage2D))iOSMGLGetProcAddress("glTexSubImage2D");
    glad_glTexSubImage3D = (decltype(glad_glTexSubImage3D))iOSMGLGetProcAddress("glTexSubImage3D");
    glad_glTransformFeedbackVaryings = (decltype(glad_glTransformFeedbackVaryings))iOSMGLGetProcAddress("glTransformFeedbackVaryings");
    glad_glTranslated = (decltype(glad_glTranslated))iOSMGLGetProcAddress("glTranslated");
    glad_glTranslatef = (decltype(glad_glTranslatef))iOSMGLGetProcAddress("glTranslatef");
    glad_glUniform1f = (decltype(glad_glUniform1f))iOSMGLGetProcAddress("glUniform1f");
    glad_glUniform1fv = (decltype(glad_glUniform1fv))iOSMGLGetProcAddress("glUniform1fv");
    glad_glUniform1i = (decltype(glad_glUniform1i))iOSMGLGetProcAddress("glUniform1i");
    glad_glUniform1iv = (decltype(glad_glUniform1iv))iOSMGLGetProcAddress("glUniform1iv");
    glad_glUniform1ui = (decltype(glad_glUniform1ui))iOSMGLGetProcAddress("glUniform1ui");
    glad_glUniform1uiv = (decltype(glad_glUniform1uiv))iOSMGLGetProcAddress("glUniform1uiv");
    glad_glUniform2f = (decltype(glad_glUniform2f))iOSMGLGetProcAddress("glUniform2f");
    glad_glUniform2fv = (decltype(glad_glUniform2fv))iOSMGLGetProcAddress("glUniform2fv");
    glad_glUniform2i = (decltype(glad_glUniform2i))iOSMGLGetProcAddress("glUniform2i");
    glad_glUniform2iv = (decltype(glad_glUniform2iv))iOSMGLGetProcAddress("glUniform2iv");
    glad_glUniform2ui = (decltype(glad_glUniform2ui))iOSMGLGetProcAddress("glUniform2ui");
    glad_glUniform2uiv = (decltype(glad_glUniform2uiv))iOSMGLGetProcAddress("glUniform2uiv");
    glad_glUniform3f = (decltype(glad_glUniform3f))iOSMGLGetProcAddress("glUniform3f");
    glad_glUniform3fv = (decltype(glad_glUniform3fv))iOSMGLGetProcAddress("glUniform3fv");
    glad_glUniform3i = (decltype(glad_glUniform3i))iOSMGLGetProcAddress("glUniform3i");
    glad_glUniform3iv = (decltype(glad_glUniform3iv))iOSMGLGetProcAddress("glUniform3iv");
    glad_glUniform3ui = (decltype(glad_glUniform3ui))iOSMGLGetProcAddress("glUniform3ui");
    glad_glUniform3uiv = (decltype(glad_glUniform3uiv))iOSMGLGetProcAddress("glUniform3uiv");
    glad_glUniform4f = (decltype(glad_glUniform4f))iOSMGLGetProcAddress("glUniform4f");
    glad_glUniform4fv = (decltype(glad_glUniform4fv))iOSMGLGetProcAddress("glUniform4fv");
    glad_glUniform4i = (decltype(glad_glUniform4i))iOSMGLGetProcAddress("glUniform4i");
    glad_glUniform4iv = (decltype(glad_glUniform4iv))iOSMGLGetProcAddress("glUniform4iv");
    glad_glUniform4ui = (decltype(glad_glUniform4ui))iOSMGLGetProcAddress("glUniform4ui");
    glad_glUniform4uiv = (decltype(glad_glUniform4uiv))iOSMGLGetProcAddress("glUniform4uiv");
    glad_glUniformMatrix2fv = (decltype(glad_glUniformMatrix2fv))iOSMGLGetProcAddress("glUniformMatrix2fv");
    glad_glUniformMatrix2x3fv = (decltype(glad_glUniformMatrix2x3fv))iOSMGLGetProcAddress("glUniformMatrix2x3fv");
    glad_glUniformMatrix2x4fv = (decltype(glad_glUniformMatrix2x4fv))iOSMGLGetProcAddress("glUniformMatrix2x4fv");
    glad_glUniformMatrix3fv = (decltype(glad_glUniformMatrix3fv))iOSMGLGetProcAddress("glUniformMatrix3fv");
    glad_glUniformMatrix3x2fv = (decltype(glad_glUniformMatrix3x2fv))iOSMGLGetProcAddress("glUniformMatrix3x2fv");
    glad_glUniformMatrix3x4fv = (decltype(glad_glUniformMatrix3x4fv))iOSMGLGetProcAddress("glUniformMatrix3x4fv");
    glad_glUniformMatrix4fv = (decltype(glad_glUniformMatrix4fv))iOSMGLGetProcAddress("glUniformMatrix4fv");
    glad_glUniformMatrix4x2fv = (decltype(glad_glUniformMatrix4x2fv))iOSMGLGetProcAddress("glUniformMatrix4x2fv");
    glad_glUniformMatrix4x3fv = (decltype(glad_glUniformMatrix4x3fv))iOSMGLGetProcAddress("glUniformMatrix4x3fv");
    glad_glUnmapBuffer = (decltype(glad_glUnmapBuffer))iOSMGLGetProcAddress("glUnmapBuffer");
    glad_glUseProgram = (decltype(glad_glUseProgram))iOSMGLGetProcAddress("glUseProgram");
    glad_glValidateProgram = (decltype(glad_glValidateProgram))iOSMGLGetProcAddress("glValidateProgram");
    glad_glVertex2d = (decltype(glad_glVertex2d))iOSMGLGetProcAddress("glVertex2d");
    glad_glVertex2dv = (decltype(glad_glVertex2dv))iOSMGLGetProcAddress("glVertex2dv");
    glad_glVertex2f = (decltype(glad_glVertex2f))iOSMGLGetProcAddress("glVertex2f");
    glad_glVertex2fv = (decltype(glad_glVertex2fv))iOSMGLGetProcAddress("glVertex2fv");
    glad_glVertex2i = (decltype(glad_glVertex2i))iOSMGLGetProcAddress("glVertex2i");
    glad_glVertex2iv = (decltype(glad_glVertex2iv))iOSMGLGetProcAddress("glVertex2iv");
    glad_glVertex2s = (decltype(glad_glVertex2s))iOSMGLGetProcAddress("glVertex2s");
    glad_glVertex2sv = (decltype(glad_glVertex2sv))iOSMGLGetProcAddress("glVertex2sv");
    glad_glVertex3d = (decltype(glad_glVertex3d))iOSMGLGetProcAddress("glVertex3d");
    glad_glVertex3dv = (decltype(glad_glVertex3dv))iOSMGLGetProcAddress("glVertex3dv");
    glad_glVertex3f = (decltype(glad_glVertex3f))iOSMGLGetProcAddress("glVertex3f");
    glad_glVertex3fv = (decltype(glad_glVertex3fv))iOSMGLGetProcAddress("glVertex3fv");
    glad_glVertex3i = (decltype(glad_glVertex3i))iOSMGLGetProcAddress("glVertex3i");
    glad_glVertex3iv = (decltype(glad_glVertex3iv))iOSMGLGetProcAddress("glVertex3iv");
    glad_glVertex3s = (decltype(glad_glVertex3s))iOSMGLGetProcAddress("glVertex3s");
    glad_glVertex3sv = (decltype(glad_glVertex3sv))iOSMGLGetProcAddress("glVertex3sv");
    glad_glVertex4d = (decltype(glad_glVertex4d))iOSMGLGetProcAddress("glVertex4d");
    glad_glVertex4dv = (decltype(glad_glVertex4dv))iOSMGLGetProcAddress("glVertex4dv");
    glad_glVertex4f = (decltype(glad_glVertex4f))iOSMGLGetProcAddress("glVertex4f");
    glad_glVertex4fv = (decltype(glad_glVertex4fv))iOSMGLGetProcAddress("glVertex4fv");
    glad_glVertex4i = (decltype(glad_glVertex4i))iOSMGLGetProcAddress("glVertex4i");
    glad_glVertex4iv = (decltype(glad_glVertex4iv))iOSMGLGetProcAddress("glVertex4iv");
    glad_glVertex4s = (decltype(glad_glVertex4s))iOSMGLGetProcAddress("glVertex4s");
    glad_glVertex4sv = (decltype(glad_glVertex4sv))iOSMGLGetProcAddress("glVertex4sv");
    glad_glVertexAttrib1d = (decltype(glad_glVertexAttrib1d))iOSMGLGetProcAddress("glVertexAttrib1d");
    glad_glVertexAttrib1dv = (decltype(glad_glVertexAttrib1dv))iOSMGLGetProcAddress("glVertexAttrib1dv");
    glad_glVertexAttrib1f = (decltype(glad_glVertexAttrib1f))iOSMGLGetProcAddress("glVertexAttrib1f");
    glad_glVertexAttrib1fv = (decltype(glad_glVertexAttrib1fv))iOSMGLGetProcAddress("glVertexAttrib1fv");
    glad_glVertexAttrib1s = (decltype(glad_glVertexAttrib1s))iOSMGLGetProcAddress("glVertexAttrib1s");
    glad_glVertexAttrib1sv = (decltype(glad_glVertexAttrib1sv))iOSMGLGetProcAddress("glVertexAttrib1sv");
    glad_glVertexAttrib2d = (decltype(glad_glVertexAttrib2d))iOSMGLGetProcAddress("glVertexAttrib2d");
    glad_glVertexAttrib2dv = (decltype(glad_glVertexAttrib2dv))iOSMGLGetProcAddress("glVertexAttrib2dv");
    glad_glVertexAttrib2f = (decltype(glad_glVertexAttrib2f))iOSMGLGetProcAddress("glVertexAttrib2f");
    glad_glVertexAttrib2fv = (decltype(glad_glVertexAttrib2fv))iOSMGLGetProcAddress("glVertexAttrib2fv");
    glad_glVertexAttrib2s = (decltype(glad_glVertexAttrib2s))iOSMGLGetProcAddress("glVertexAttrib2s");
    glad_glVertexAttrib2sv = (decltype(glad_glVertexAttrib2sv))iOSMGLGetProcAddress("glVertexAttrib2sv");
    glad_glVertexAttrib3d = (decltype(glad_glVertexAttrib3d))iOSMGLGetProcAddress("glVertexAttrib3d");
    glad_glVertexAttrib3dv = (decltype(glad_glVertexAttrib3dv))iOSMGLGetProcAddress("glVertexAttrib3dv");
    glad_glVertexAttrib3f = (decltype(glad_glVertexAttrib3f))iOSMGLGetProcAddress("glVertexAttrib3f");
    glad_glVertexAttrib3fv = (decltype(glad_glVertexAttrib3fv))iOSMGLGetProcAddress("glVertexAttrib3fv");
    glad_glVertexAttrib3s = (decltype(glad_glVertexAttrib3s))iOSMGLGetProcAddress("glVertexAttrib3s");
    glad_glVertexAttrib3sv = (decltype(glad_glVertexAttrib3sv))iOSMGLGetProcAddress("glVertexAttrib3sv");
    glad_glVertexAttrib4Nbv = (decltype(glad_glVertexAttrib4Nbv))iOSMGLGetProcAddress("glVertexAttrib4Nbv");
    glad_glVertexAttrib4Niv = (decltype(glad_glVertexAttrib4Niv))iOSMGLGetProcAddress("glVertexAttrib4Niv");
    glad_glVertexAttrib4Nsv = (decltype(glad_glVertexAttrib4Nsv))iOSMGLGetProcAddress("glVertexAttrib4Nsv");
    glad_glVertexAttrib4Nub = (decltype(glad_glVertexAttrib4Nub))iOSMGLGetProcAddress("glVertexAttrib4Nub");
    glad_glVertexAttrib4Nubv = (decltype(glad_glVertexAttrib4Nubv))iOSMGLGetProcAddress("glVertexAttrib4Nubv");
    glad_glVertexAttrib4Nuiv = (decltype(glad_glVertexAttrib4Nuiv))iOSMGLGetProcAddress("glVertexAttrib4Nuiv");
    glad_glVertexAttrib4Nusv = (decltype(glad_glVertexAttrib4Nusv))iOSMGLGetProcAddress("glVertexAttrib4Nusv");
    glad_glVertexAttrib4bv = (decltype(glad_glVertexAttrib4bv))iOSMGLGetProcAddress("glVertexAttrib4bv");
    glad_glVertexAttrib4d = (decltype(glad_glVertexAttrib4d))iOSMGLGetProcAddress("glVertexAttrib4d");
    glad_glVertexAttrib4dv = (decltype(glad_glVertexAttrib4dv))iOSMGLGetProcAddress("glVertexAttrib4dv");
    glad_glVertexAttrib4f = (decltype(glad_glVertexAttrib4f))iOSMGLGetProcAddress("glVertexAttrib4f");
    glad_glVertexAttrib4fv = (decltype(glad_glVertexAttrib4fv))iOSMGLGetProcAddress("glVertexAttrib4fv");
    glad_glVertexAttrib4iv = (decltype(glad_glVertexAttrib4iv))iOSMGLGetProcAddress("glVertexAttrib4iv");
    glad_glVertexAttrib4s = (decltype(glad_glVertexAttrib4s))iOSMGLGetProcAddress("glVertexAttrib4s");
    glad_glVertexAttrib4sv = (decltype(glad_glVertexAttrib4sv))iOSMGLGetProcAddress("glVertexAttrib4sv");
    glad_glVertexAttrib4ubv = (decltype(glad_glVertexAttrib4ubv))iOSMGLGetProcAddress("glVertexAttrib4ubv");
    glad_glVertexAttrib4uiv = (decltype(glad_glVertexAttrib4uiv))iOSMGLGetProcAddress("glVertexAttrib4uiv");
    glad_glVertexAttrib4usv = (decltype(glad_glVertexAttrib4usv))iOSMGLGetProcAddress("glVertexAttrib4usv");
    glad_glVertexAttribI1i = (decltype(glad_glVertexAttribI1i))iOSMGLGetProcAddress("glVertexAttribI1i");
    glad_glVertexAttribI1iv = (decltype(glad_glVertexAttribI1iv))iOSMGLGetProcAddress("glVertexAttribI1iv");
    glad_glVertexAttribI1ui = (decltype(glad_glVertexAttribI1ui))iOSMGLGetProcAddress("glVertexAttribI1ui");
    glad_glVertexAttribI1uiv = (decltype(glad_glVertexAttribI1uiv))iOSMGLGetProcAddress("glVertexAttribI1uiv");
    glad_glVertexAttribI2i = (decltype(glad_glVertexAttribI2i))iOSMGLGetProcAddress("glVertexAttribI2i");
    glad_glVertexAttribI2iv = (decltype(glad_glVertexAttribI2iv))iOSMGLGetProcAddress("glVertexAttribI2iv");
    glad_glVertexAttribI2ui = (decltype(glad_glVertexAttribI2ui))iOSMGLGetProcAddress("glVertexAttribI2ui");
    glad_glVertexAttribI2uiv = (decltype(glad_glVertexAttribI2uiv))iOSMGLGetProcAddress("glVertexAttribI2uiv");
    glad_glVertexAttribI3i = (decltype(glad_glVertexAttribI3i))iOSMGLGetProcAddress("glVertexAttribI3i");
    glad_glVertexAttribI3iv = (decltype(glad_glVertexAttribI3iv))iOSMGLGetProcAddress("glVertexAttribI3iv");
    glad_glVertexAttribI3ui = (decltype(glad_glVertexAttribI3ui))iOSMGLGetProcAddress("glVertexAttribI3ui");
    glad_glVertexAttribI3uiv = (decltype(glad_glVertexAttribI3uiv))iOSMGLGetProcAddress("glVertexAttribI3uiv");
    glad_glVertexAttribI4bv = (decltype(glad_glVertexAttribI4bv))iOSMGLGetProcAddress("glVertexAttribI4bv");
    glad_glVertexAttribI4i = (decltype(glad_glVertexAttribI4i))iOSMGLGetProcAddress("glVertexAttribI4i");
    glad_glVertexAttribI4iv = (decltype(glad_glVertexAttribI4iv))iOSMGLGetProcAddress("glVertexAttribI4iv");
    glad_glVertexAttribI4sv = (decltype(glad_glVertexAttribI4sv))iOSMGLGetProcAddress("glVertexAttribI4sv");
    glad_glVertexAttribI4ubv = (decltype(glad_glVertexAttribI4ubv))iOSMGLGetProcAddress("glVertexAttribI4ubv");
    glad_glVertexAttribI4ui = (decltype(glad_glVertexAttribI4ui))iOSMGLGetProcAddress("glVertexAttribI4ui");
    glad_glVertexAttribI4uiv = (decltype(glad_glVertexAttribI4uiv))iOSMGLGetProcAddress("glVertexAttribI4uiv");
    glad_glVertexAttribI4usv = (decltype(glad_glVertexAttribI4usv))iOSMGLGetProcAddress("glVertexAttribI4usv");
    glad_glVertexAttribIPointer = (decltype(glad_glVertexAttribIPointer))iOSMGLGetProcAddress("glVertexAttribIPointer");
    glad_glVertexAttribPointer = (decltype(glad_glVertexAttribPointer))iOSMGLGetProcAddress("glVertexAttribPointer");
    glad_glVertexPointer = (decltype(glad_glVertexPointer))iOSMGLGetProcAddress("glVertexPointer");
    glad_glViewport = (decltype(glad_glViewport))iOSMGLGetProcAddress("glViewport");
    glad_glWindowPos2d = (decltype(glad_glWindowPos2d))iOSMGLGetProcAddress("glWindowPos2d");
    glad_glWindowPos2dv = (decltype(glad_glWindowPos2dv))iOSMGLGetProcAddress("glWindowPos2dv");
    glad_glWindowPos2f = (decltype(glad_glWindowPos2f))iOSMGLGetProcAddress("glWindowPos2f");
    glad_glWindowPos2fv = (decltype(glad_glWindowPos2fv))iOSMGLGetProcAddress("glWindowPos2fv");
    glad_glWindowPos2i = (decltype(glad_glWindowPos2i))iOSMGLGetProcAddress("glWindowPos2i");
    glad_glWindowPos2iv = (decltype(glad_glWindowPos2iv))iOSMGLGetProcAddress("glWindowPos2iv");
    glad_glWindowPos2s = (decltype(glad_glWindowPos2s))iOSMGLGetProcAddress("glWindowPos2s");
    glad_glWindowPos2sv = (decltype(glad_glWindowPos2sv))iOSMGLGetProcAddress("glWindowPos2sv");
    glad_glWindowPos3d = (decltype(glad_glWindowPos3d))iOSMGLGetProcAddress("glWindowPos3d");
    glad_glWindowPos3dv = (decltype(glad_glWindowPos3dv))iOSMGLGetProcAddress("glWindowPos3dv");
    glad_glWindowPos3f = (decltype(glad_glWindowPos3f))iOSMGLGetProcAddress("glWindowPos3f");
    glad_glWindowPos3fv = (decltype(glad_glWindowPos3fv))iOSMGLGetProcAddress("glWindowPos3fv");
    glad_glWindowPos3i = (decltype(glad_glWindowPos3i))iOSMGLGetProcAddress("glWindowPos3i");
    glad_glWindowPos3iv = (decltype(glad_glWindowPos3iv))iOSMGLGetProcAddress("glWindowPos3iv");
    glad_glWindowPos3s = (decltype(glad_glWindowPos3s))iOSMGLGetProcAddress("glWindowPos3s");
    glad_glWindowPos3sv = (decltype(glad_glWindowPos3sv))iOSMGLGetProcAddress("glWindowPos3sv");

    }
    // OES-suffixed entry points: pddi calls glBindVertexArrayOES etc; MGL
    // exports the core (non-OES) names with identical signatures. glad's ES2
    // loader fills neither, so resolve them straight from MGL.
    if (!glad_glBindVertexArrayOES)    glad_glBindVertexArrayOES    = (PFNGLBINDVERTEXARRAYOESPROC)iOSMGLGetProcAddress("glBindVertexArray");
    if (!glad_glGenVertexArraysOES)    glad_glGenVertexArraysOES    = (PFNGLGENVERTEXARRAYSOESPROC)iOSMGLGetProcAddress("glGenVertexArrays");
    if (!glad_glDeleteVertexArraysOES) glad_glDeleteVertexArraysOES = (PFNGLDELETEVERTEXARRAYSOESPROC)iOSMGLGetProcAddress("glDeleteVertexArrays");
    if (!glad_glIsVertexArrayOES)      glad_glIsVertexArrayOES      = (PFNGLISVERTEXARRAYOESPROC)iOSMGLGetProcAddress("glIsVertexArray");
#else
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, bpp == 16 ? 5 : 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, bpp == 16 ? 6 : 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, bpp == 16 ? 5 : 8);
    if (init->bufferMask & PDDI_BUFFER_DEPTH)
#ifdef __SWITCH__
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, init->bufferMask & PDDI_BUFFER_STENCIL ? 24 : 32);
#else
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
#endif
    else
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
    if (init->bufferMask & PDDI_BUFFER_STENCIL)
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    else
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
#ifndef RAD_VITA
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, true);
#ifdef RAD_DEBUG
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
#endif
#endif

    prevRC = SDL_GL_GetCurrentContext();
    hRC = SDL_GL_CreateContext(win);
    if (!hRC)
        SDL_Log("SDL_GL_CreateContext() error: %s", SDL_GetError());
    PDDIASSERT(hRC);

#ifdef RAD_CG
    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress))
        return false;
#else
    if (!gladLoadGLES2Loader((GLADloadproc)SDL_GL_GetProcAddress))
        return false;
#endif

#endif
    char* glVendor   = (char*)glGetString(GL_VENDOR);
    char* glRenderer = (char*)glGetString(GL_RENDERER);
    char* glVersion  = (char*)glGetString(GL_VERSION);
    char* glExtensions = (char*)glGetString(GL_EXTENSIONS);

    static bool doExtensions = false;

    if(doExtensions)
    {
        doExtensions = false;
        char* buffer = new char[strlen(glExtensions) + 2];
        strcpy(buffer, glExtensions);

        char* walk = buffer;
        char* last = buffer;
        while(*walk)
        {
            if(*walk == ' ')
            {
                *walk = 0;
                SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "%s", last);
                last = walk+1;
            }
            walk++;
        }
        SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "%s", last);
    }

    extBGRA = CheckExtension("GL_EXT_bgra") || CheckExtension("GL_EXT_texture_format_BGRA8888");

    SDL_Log("OpenGL - Vendor: %s, Renderer: %s, Version: %s",glVendor,glRenderer,glVersion);

#if defined(RAD_IOS)
    // Diagnostico: volcar el contexto GL del dispositivo real a un archivo en
    // Documents/ (leible via `devicectl device copy from`).
    {
        const char* docs = iOSGetDocumentsDir();
        char path[ 1024 ] = { 0 };
        if( docs != NULL && docs[ 0 ] != '\0' ) {
            snprintf( path, sizeof( path ), "%s/srr2_gl.txt", docs );
        } else {
            snprintf( path, sizeof( path ), "/tmp/srr2_gl.txt" );
        }
        FILE* diag = fopen( path, "w" );
        if( diag != NULL ) {
            int drawW = 0, drawH = 0;
            #if defined(RAD_IOS)
            iOSGetScreenPixelSize(&drawW, &drawH);
            #elif SDL_MAJOR_VERSION < 3
            SDL_GL_GetDrawableSize( win, &drawW, &drawH );
            #else
            SDL_GetWindowSizeInPixels( win, &drawW, &drawH );
            #endif
            fprintf( diag, "Vendor:   %s\n", glVendor   ? glVendor   : "(null)" );
            fprintf( diag, "Renderer: %s\n", glRenderer ? glRenderer : "(null)" );
            fprintf( diag, "Version:  %s\n", glVersion  ? glVersion  : "(null)" );
            fprintf( diag, "winCreate: %dx%d\n", x, y );
            fprintf( diag, "drawable:  %dx%d\n", drawW, drawH );
            fprintf( diag, "pddi:      %dx%d\n", winWidth, winHeight );
            fclose( diag );
        }
    }
#endif

#if defined RAD_DEBUG && !defined RAD_VITA
    glEnable(GL_DEBUG_OUTPUT_KHR);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_KHR);
    glDebugMessageCallback(MessageCallback, NULL);
#endif

    return true;
}

pddiDisplayInfo* pglDisplay ::GetDisplayInfo(void)
{
    return displayInfo;
}

unsigned pglDisplay ::GetFreeTextureMem()
{
    return unsigned(-1);
}

unsigned pglDisplay ::GetBufferMask()
{
    return unsigned(-1);
}

int pglDisplay ::GetHeight()
{
    return winHeight;
}

int pglDisplay ::GetWidth()
{
    return winWidth;
}

int pglDisplay::GetDepth()
{
    return winBitDepth;
}

pddiDisplayMode pglDisplay::GetDisplayMode(void)
{
    return mode;
}

int pglDisplay::GetNumColourBuffer(void)
{
    return 2;
}

void pglDisplay::GetGamma(float* r, float* g, float* b)
{
    *r = gammaR;
    *g = gammaG;
    *b = gammaB;
}

void pglDisplay::SetGamma(float r, float g, float b)
{
    gammaR = r;
    gammaG = g;
    gammaB = b;

    Uint16 gamma[3][256];

    double igr = 1.0 / (double)r;
    double igg = 1.0 / (double)g;
    double igb = 1.0 / (double)b;

    const double n = 1.0 / 65535.0;

    for(int i=0; i < 256; i++)
    {
        double gcr = pow((double)initialGammaRamp[0][i]   * n, igr);
        double gcg = pow((double)initialGammaRamp[1][i] * n, igg);
        double gcb = pow((double)initialGammaRamp[2][i]  * n, igb);

        gamma[0][i] =   (Uint16)(65535.0 * ((1.0 < gcr) ? 1.0 : gcr));
        gamma[1][i] = (Uint16)(65535.0 * ((1.0 < gcg) ? 1.0 : gcg));
        gamma[2][i] =  (Uint16)(65535.0 * ((1.0 < gcb) ? 1.0 : gcb));
    }

#if SDL_MAJOR_VERSION < 3
    SDL_SetWindowGammaRamp(win, gamma[0], gamma[1], gamma[2]);
#endif
}

void pglDisplay::SwapBuffers(void)
{
#if defined(RAD_IOS)
    if (s_iosMGLContext)
    {
        MGLswapBuffers(NULL);
        iOSRecordGameFrame();
    }
#else
    SDL_GL_SwapWindow(win);
#endif
    reset = false;
    #ifdef RAD_ANDROID
    /*
     * Manual 60 FPS cap for Android.
     *
     * This is intentionally independent from display refresh rate.
     * It prevents 90 Hz / 120 Hz devices from running the game logic too fast.
     *
     * m_only60 must remain the switch:
     * - true  = gameplay cap at 60 FPS
     * - false = FMV/cinematics or special states, do not force 60 here
     */
    static Uint64 sCapFreq = 0;
    static Uint64 sLastFrameTime = 0;

    if (sCapFreq == 0)
    {
        sCapFreq = SDL_GetPerformanceFrequency();
    }

    if (m_only60)
    {
        const Uint64 targetTicks =
            (Uint64)((double)sCapFreq / 60.0 + 0.5);

        Uint64 now = SDL_GetPerformanceCounter();

        if (sLastFrameTime == 0)
        {
            sLastFrameTime = now;
        }
        else
        {
            const Uint64 targetTime = sLastFrameTime + targetTicks;

            if (now < targetTime)
            {
                Uint64 remainingTicks = targetTime - now;

                double remainingMs =
                    (double)remainingTicks * 1000.0 / (double)sCapFreq;

                /*
                 * Coarse wait.
                 * Sleep most of the remaining time, leaving a small margin
                 * because SDL_Delay is not perfectly precise on Android.
                 */
                if (remainingMs > 2.0)
                {
                    SDL_Delay((Uint32)(remainingMs - 1.0));
                }

                /*
                 * Fine wait.
                 * Not a pure busy-wait: SDL_Delay(0) yields to the scheduler.
                 * This usually only runs for a tiny fraction of a millisecond.
                 */
                while (SDL_GetPerformanceCounter() < targetTime)
                {
                    SDL_Delay(0);
                }

                now = SDL_GetPerformanceCounter();
            }

            /*
             * Important:
             * Use the real current time, not targetTime.
             *
             * This avoids "catch-up" frames after a small hitch.
             * For this old game, we prefer never producing a too-fast frame.
             */
            sLastFrameTime = now;
        }
    }
    else
    {
        /*
         * If 60 FPS cap is disabled, reset the cap timer.
         *
         * This is important for FMV/cinematics:
         * when the game returns from a 30 FPS movie to gameplay,
         * we do not want to carry an old timestamp.
         */
        sLastFrameTime = 0;
    }
#endif

// Contador fps para logcat
#if defined(RAD_ANDROID) && defined(RAD_DEBUG)
    static int frames = 0;
    static Uint32 lastTick = 0;

    Uint32 t = SDL_GetTicks();

    if (lastTick == 0)
    {
        lastTick = t;
    }

    frames++;

    Uint32 elapsed = t - lastTick;

    if (elapsed >= 1000)
    {
        double fps = (double)frames * 1000.0 / (double)elapsed;

        SDL_Log("[FPS] %.2f", fps);

        frames = 0;
        lastTick = t;
    }
#endif
}

    
unsigned pglDisplay::Screenshot(pddiColour* buffer, int nBytes)
{
    if(nBytes < (winHeight * winWidth * 4))
        return 0;

    glReadPixels(0, 0,  winWidth, winHeight, GL_BGRA_EXT, GL_UNSIGNED_BYTE, buffer);

    unsigned tmp[2048];
    PDDIASSERT(winWidth < 2048);

    for(int i = 0; i < winHeight / 2; i++)
    {
        pddiColour* a = buffer + (i * winWidth);
        pddiColour* b = buffer + (((winHeight - 1) - i) * winWidth);
        memcpy(tmp, a, winWidth * 4);
        memcpy(a, b, winWidth * 4);
        memcpy(b, tmp, winWidth * 4);
    }

    return winHeight * winWidth * 4;
}

void pglDisplay::BeginTiming()
{
    beginTime = (float)SDL_GetTicks();
}

float pglDisplay::EndTiming()
{
    return (float)SDL_GetTicks() - beginTime;
}

void pglDisplay::BeginContext(void)
{
#if !defined(RAD_IOS)
    prevRC = SDL_GL_GetCurrentContext();
    PDDIASSERT(prevRC != hRC);
    int error = SDL_GL_MakeCurrent(win, (SDL_GLContext)hRC);
    PDDIASSERT(!error);
#endif
}

void pglDisplay::EndContext(void)
{
#if !defined(RAD_IOS)
    int error = SDL_GL_MakeCurrent(win, (SDL_GLContext)prevRC);
    PDDIASSERT(!error);
#endif
}

