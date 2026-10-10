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
// SDL_main runs a continuous C++ loop inside UIKit's launch callback. Drain
// temporary Cocoa/Metal objects every frame instead of at SDL_main return.
extern "C" void* iOSBeginFrameAutoreleasePool()
{
    return [[NSAutoreleasePool alloc] init];
}
extern "C" void iOSEndFrameAutoreleasePool(void* pool)
{
    [(NSAutoreleasePool*)pool drain];
}

// Kept independent of the frame loop so hangs leave memory samples behind.
extern "C" void iOSSampleRuntimeHealth();
extern "C" void iOSDiagnosticEvent(const char*, const char*);
extern "C" void iOSDiagnosticFlush();
extern "C" void iOSStartHealthMonitor()
{
    static dispatch_source_t timer;
    static id warningObserver, backgroundObserver, foregroundObserver;
    if (timer) return;
    dispatch_queue_t queue = dispatch_get_global_queue(QOS_CLASS_UTILITY, 0);
    timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, queue);
    dispatch_source_set_timer(timer, dispatch_time(DISPATCH_TIME_NOW, 0),
                              5 * NSEC_PER_SEC, NSEC_PER_SEC / 2);
    dispatch_source_set_event_handler(timer, ^{ iOSSampleRuntimeHealth(); });
    dispatch_resume(timer);
    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
    warningObserver = [center addObserverForName:UIApplicationDidReceiveMemoryWarningNotification
        object:nil queue:nil usingBlock:^(NSNotification*) {
            iOSDiagnosticEvent("memory-warning", "source=UIKit");
            iOSSampleRuntimeHealth();
        }];
    backgroundObserver = [center addObserverForName:UIApplicationDidEnterBackgroundNotification
        object:nil queue:nil usingBlock:^(NSNotification*) {
            iOSDiagnosticEvent("lifecycle", "state=background");
            iOSDiagnosticFlush();
        }];
    foregroundObserver = [center addObserverForName:UIApplicationWillEnterForegroundNotification
        object:nil queue:nil usingBlock:^(NSNotification*) {
            iOSDiagnosticEvent("lifecycle", "state=foreground");
        }];
}
#endif // RAD_IOS
