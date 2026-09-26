//=============================================================================
// touchplatformpaths.h — resuelve el directorio raíz escribible donde se
// guardan los assets de los controles táctiles y su configuración.
//
// Android: external storage (SDL_AndroidGetExternalStoragePath) + "touch_controls".
// iOS:     Documents/ (NSDocumentDirectory) → donde el jugador coloca los assets.
// Desktop: "." (relativo al CWD, como antes).
//
// Devuelve un puntero a un buffer estatico; la llamada debe consumirlo antes
// de volver a invocar.
//=============================================================================
#ifndef TOUCHPLATFORMPATHS_H
#define TOUCHPLATFORMPATHS_H

#ifdef __cplusplus
extern "C" {
#endif

// Implementado en:
//   RAD_ANDROID → usa SDL_AndroidGetExternalStoragePath (via <SDL_system.h>)
//   RAD_IOS      → code/main/iosdocuments.mm (NSDocumentDirectory)
//   otro         → "."
const char* GetTouchWriteBasePath(void);

#ifdef __cplusplus
}
#endif

#endif // TOUCHPLATFORMPATHS_H
