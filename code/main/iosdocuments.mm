//=============================================================================
// iosdocuments.m — bridge Objective-C que expone el directorio Documents/ de la
// app al motor C++ (RAD_IOS). El motor no debe depender de <UIKit/UIKit.h>;
// este archivo aísla la unica dependencia de UIKit del port.
//
// Devuelve un puntero a un buffer estatico (vida del proceso). El motor lo usa
// una sola vez al arrancar para hacer chdir(Documents), asi que no necesita
// reciclarse despues.
//=============================================================================
#ifdef RAD_IOS
#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#include <limits.h>   // PATH_MAX
#include <string.h>   // strncpy

// Tamano del drawable real (pixeles) de la pantalla principal, en orientacion
// actual. iOS usa puntos (points) con una escala (scale); el framebuffer GL de
// SDL se crea a pantalla completa, pero SDL_GetWindowSize* reporta el tamano
// LOGICO del window (800x600). Este helper da los pixeles fisicos.
extern "C" void iOSGetScreenPixelSize(int* outW, int* outH)
{
    if( outW == NULL || outH == NULL ) { return; }

    UIScreen* screen = [ UIScreen mainScreen ];
    // bounds ya viene en la orientacion ACTUAL de la UI (no nativa). La app es
    // landscape-only, asi que bounds es la resolucion landscape (puntos), y
    // multiplicando por scale obtenemos el tamano en pixeles fisicos del drawable
    // en la orientacion correcta. nativeBounds es portrait por defecto y nos
    // daria 1320x2868 (mal para una app landscape).
    CGRect nb = [ screen bounds ];
    CGFloat scale = [ screen scale ];
    *outW = (int)lroundf( nb.size.width  * scale );
    *outH = (int)lroundf( nb.size.height * scale );
}

extern "C" const char* iOSGetDocumentsDir(void)
{
    // NSDocumentDirectory es el "Documents/" de la app: escribible y persistente.
    // Es donde el jugador coloca los assets (flujo W3) y donde el motor guarda
    // partidas/configuracion.
    static char s_documentsPath[ PATH_MAX ];
    static bool s_initialized = false;

    if( !s_initialized )
    {
        NSArray* dirs = NSSearchPathForDirectoriesInDomains(
            NSDocumentDirectory, NSUserDomainMask, YES );
        if( dirs != nil && [ dirs count ] > 0 )
        {
            NSString* path = [ dirs objectAtIndex: 0 ];
            const char* cpath = [ path fileSystemRepresentation ];
            if( cpath != NULL )
            {
                strncpy( s_documentsPath, cpath, sizeof( s_documentsPath ) - 1 );
                s_documentsPath[ sizeof( s_documentsPath ) - 1 ] = '\0';
                s_initialized = true;
            }
        }
    }

    return s_initialized ? s_documentsPath : NULL;
}
#endif // RAD_IOS
