//=============================================================================
// iOS app shell stub.
//
// Este .cpp NO define main(). El main() real vive en libmain.a (compilado desde
// win32main.cpp, cuyo `main` se renombra a `SDL_main` por el macro en
// SDL_main.h). SDL2 uikit (SDL2main) proporciona el bootstrap UIKit que llama
// a SDL_main(argc, argv).
//
// El objetivo de esta unidad de traduccion es solo que CMake/Xcode creen el
// target ejecutable SimpsonsIOS. No hace falta ninguna logica aqui.
//
// Ademas registra un manejador de señales (SIGABRT/SIGSEGV/SIGBUS/SIGILL) y
// std::set_terminate que vuelca un backtrace a <Documents>/crash.txt, para
// poder diagnosticar un crash de arranque en el dispositivo real (que sale de
// forma silenciosa en la pantalla oscura).
//=============================================================================

#if defined(RAD_IOS)
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <execinfo.h>
#include <exception>
#include <stdio.h>
#include <time.h>

extern "C" const char* iOSGetDocumentsDir(void);

static void WriteCrashDump(const char* header, void** frames, int nframes)
{
    const char* docs = iOSGetDocumentsDir();
    char path[ 1024 ];
    if( docs == NULL || docs[ 0 ] == '\0' ) {
        snprintf( path, sizeof( path ), "/tmp/srr2_crash.txt" );
    } else {
        snprintf( path, sizeof( path ), "%s/srr2_crash.txt", docs );
    }

    // El sandbox /tmp (container tmp/) tambien es accesible via copy from.
    static const char* tmpPath = "/tmp/crash.txt";

    FILE* f = fopen( path, "a" );
    if( f == NULL ) {
        f = fopen( tmpPath, "a" );
        if( f == NULL ) { return; }
    }

    time_t now = time( NULL );
    fprintf( f, "\n===== Crash dump %s @ %s =====\n", header, ctime( &now ) );

    if( frames != NULL && nframes > 0 ) {
        fprintf( f, "Backtrace (%d frames):\n", nframes );
        char** syms = backtrace_symbols( frames, nframes );
        if( syms != NULL ) {
            for( int i = 0; i < nframes; i++ ) {
                fprintf( f, "  #%02d %s\n", i, syms[ i ] );
            }
            free( syms );
        }
    } else {
        fprintf( f, "No backtrace captured.\n" );
    }

    // Intenta capturar el motivo del std::exception
    fflush( f );
    fclose( f );
}

static void SignalHandler(int sig)
{
    void* frames[ 64 ];
    int n = backtrace( frames, 64 );
    char header[ 64 ];
    snprintf( header, sizeof( header ), "signal %d", sig );
    WriteCrashDump( header, frames, n );
    // Re-raise con la disposicion por defecto para que el sistema registre el crash.
    signal( sig, SIG_DFL );
    raise( sig );
}

static void TerminateHandler()
{
    void* frames[ 64 ];
    int n = backtrace( frames, 64 );
    const std::exception* ex = NULL;
    std::exception_ptr eptr = std::current_exception();
    if( eptr != NULL ) {
        try { std::rethrow_exception( eptr ); }
        catch( const std::exception& e ) { ex = &e; }
        catch( ... ) {}
    }

    char header[ 256 ];
    if( ex != NULL ) {
        snprintf( header, sizeof( header ), "std::terminate: %s", ex->what() ? ex->what() : "(null)" );
    } else {
        snprintf( header, sizeof( header ), "std::terminate (no exception detail)" );
    }
    WriteCrashDump( header, frames, n );
    abort();
}

namespace srr2_ios {
    int iosshell_anchor = 0;

    __attribute__((constructor))
    void InstallCrashHandlers()
    {
        // Siempre instalar en iOS (sideload): un crash de arranque sale de forma
        // silenciosa, necesitamos el dump para diagnosticar.
        signal( SIGABRT, SignalHandler );
        signal( SIGSEGV, SignalHandler );
        signal( SIGBUS,  SignalHandler );
        signal( SIGILL,  SignalHandler );
        std::set_terminate( TerminateHandler );
    }
}
#endif
