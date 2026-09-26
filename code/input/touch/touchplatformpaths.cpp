//=============================================================================
// touchplatformpaths.cpp — implementación de GetTouchWriteBasePath por plataforma
//=============================================================================
#include "touchplatformpaths.h"

#if defined(RAD_ANDROID)
#include <SDL.h>
#include <SDL_system.h>
#elif defined(RAD_IOS)
extern "C" const char* iOSGetDocumentsDir(void);   // bridge (iosdocuments.mm)
#endif

const char* GetTouchWriteBasePath(void)
{
#if defined(RAD_ANDROID)
    {
        const char* ext = SDL_AndroidGetExternalStoragePath();
        if (ext && ext[ 0 ] != '\0') {
            return ext;
        }
        const char* internal = SDL_AndroidGetInternalStoragePath();
        if (internal && internal[ 0 ] != '\0') {
            return internal;
        }
        return ".";  // fallback
    }
#elif defined(RAD_IOS)
    {
        static const char* path = nullptr;
        if (path == nullptr) {
            const char* docs = iOSGetDocumentsDir();
            path = (docs && docs[ 0 ] != '\0') ? docs : ".";
        }
        return path;
    }
#else
    return ".";
#endif
}
