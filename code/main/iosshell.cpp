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
// std::set_terminate. Guarda señales sin allocation y backtraces de terminate, para
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
#include <fcntl.h>
#include <stdint.h>
#include <mach/mach.h>
#include <malloc/malloc.h>
#include <sys/stat.h>
#include <TargetConditionals.h>
#include <atomic>
#include "diagnostics/persistentlog.h"
#include "iossettings.h"
#include <initializer_list>
#include "iossettingspolicy.h"

static PersistentLog s_healthLog;
static std::atomic<int> s_gameContext(-1);
static std::atomic<unsigned> s_inputState(0);
extern "C" void iOSStartHealthMonitor();
extern "C" void iOSDiagnosticEvent(const char* event, const char* detail) {
    if (iOSDetailedDiagnosticsEnabled() || IOSBasicDiagnosticEvent(event)) s_healthLog.Write(event, detail);
}
extern "C" void iOSDiagnosticFlush() { s_healthLog.Flush(); }
extern "C" void iOSUpdateDiagnosticContext(int context, unsigned inputState) {
    s_gameContext.store(context);
    s_inputState.store(inputState);
}

extern "C" const char* iOSGetDocumentsDir(void);

static bool s_traceUi = false;
extern "C" bool iOSShouldTraceUiDraw() { return s_traceUi; }
extern "C" void iOSBeginUiFrame(const char* screen) {
    static char lastScreen[128] = "";
    static time_t lastSample = 0;
    time_t now = time(NULL);
    s_traceUi = strcmp(lastScreen, screen) != 0 || now - lastSample >= 5;
    if (s_traceUi) {
        snprintf(lastScreen, sizeof(lastScreen), "%s", screen);
        lastSample = now;
        char detail[192];
        snprintf(detail, sizeof(detail), "epoch=%ld screen=%s", (long)now, screen);
        iOSDiagnosticEvent("ui-frame", detail);
    }
}
extern "C" void iOSEndUiFrame() { s_traceUi = false; }

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

// Opened during normal startup, never from a failing signal handler.
static int s_crashFd = -1;
static unsigned char s_signalStack[64 * 1024];

static void SignalHandler(int sig, siginfo_t* info, void*)
{
    // No Foundation, stdio, unwinding or allocation here: any of those can
    // deadlock when the interrupted thread held a runtime/allocator lock.
    char record[128];
    size_t n = 0;
    const char prefix[] = "fatal signal=0x";
    for (size_t i = 0; i < sizeof(prefix) - 1; ++i) record[n++] = prefix[i];
    const char hex[] = "0123456789abcdef";
    record[n++] = hex[(sig >> 4) & 15];
    record[n++] = hex[sig & 15];
    const char address[] = " address=0x";
    for (size_t i = 0; i < sizeof(address) - 1; ++i) record[n++] = address[i];
    uintptr_t value = info ? reinterpret_cast<uintptr_t>(info->si_addr) : 0;
    for (int shift = sizeof(value) * 8 - 4; shift >= 0; shift -= 4)
        record[n++] = hex[(value >> shift) & 15];
    record[n++] = '\n';
    if (s_crashFd >= 0) write(s_crashFd, record, n);
    write(STDERR_FILENO, record, n);
    // SA_RESETHAND restores default disposition; deliver after unblocking.
    sigset_t unblock;
    sigemptyset(&unblock);
    sigaddset(&unblock, sig);
    sigprocmask(SIG_UNBLOCK, &unblock, NULL);
    raise(sig);
    _exit(128 + sig);
}

extern "C" void iOSClearDiagnostics() {
    s_healthLog.Clear();
    fflush(stdout); fflush(stderr);
    struct stat st;
    if (fstat(STDERR_FILENO, &st) == 0 && S_ISREG(st.st_mode)) { ftruncate(STDERR_FILENO, 0); lseek(STDERR_FILENO, 0, SEEK_SET); }
    if (s_crashFd >= 0) { ftruncate(s_crashFd, 0); lseek(s_crashFd, 0, SEEK_SET); }
    const char* docs = iOSGetDocumentsDir();
    if (docs) {
        for (const char* name : {"srr2_console.previous.log", "srr2_runtime_diag.txt", "srr2_diag.txt"}) {
            char path[1024]; snprintf(path, sizeof(path), "%s/%s", docs, name); unlink(path);
        }
    }
}

extern "C" void iOSInitializePersistentDiagnostics()
{
    const char* docs = iOSGetDocumentsDir();
    if (!docs || !docs[0]) return;
    if (!s_healthLog.Open(docs, 4 * 1024 * 1024))
        fprintf(stderr, "IOS_DIAGNOSTIC_ERROR health-log-open errno=%d directory=%s\n", errno, docs);
    char session[256];
    snprintf(session, sizeof(session), "epoch=%ld pid=%d simulator=%d build=%s_%s", (long)time(NULL), getpid(), TARGET_OS_SIMULATOR, __DATE__, __TIME__);
    iOSDiagnosticEvent("session-start", session);
    iOSStartHealthMonitor();
    char path[1024];
    snprintf(path, sizeof(path), "%s/srr2_crash.txt", docs);
    s_crashFd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0600);
#if TARGET_OS_SIMULATOR
    // Simulator capture already owns stdout/stderr. Preserve its live pipe.
    if (!getenv("IOS_PERSIST_CONSOLE")) return;
#endif
    // Retain the preceding session across a relaunch.
    char previous[1024];
    snprintf(path, sizeof(path), "%s/srr2_console.log", docs);
    snprintf(previous, sizeof(previous), "%s/srr2_console.previous.log", docs);
    rename(path, previous);
    int console = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (console >= 0) {
        dup2(console, STDOUT_FILENO);
        dup2(console, STDERR_FILENO);
        close(console);
        setvbuf(stdout, NULL, _IONBF, 0);
        setvbuf(stderr, NULL, _IONBF, 0);
    }
    fprintf(stderr, "IOS_SESSION start epoch=%ld pid=%d\n", (long)time(NULL), getpid());
}

extern "C" void iOSSampleRuntimeHealth()
{
    // Runs on an independent utility queue even if the game thread stalls.
    struct stat consoleStat;
    if (fstat(STDERR_FILENO, &consoleStat) == 0 && S_ISREG(consoleStat.st_mode) &&
        consoleStat.st_size > 8 * 1024 * 1024) {
        const char* docs = iOSGetDocumentsDir();
        if (docs) {
            char path[1024], previous[1024];
            snprintf(path, sizeof(path), "%s/srr2_console.log", docs);
            snprintf(previous, sizeof(previous), "%s/srr2_console.previous.log", docs);
            rename(path, previous);
            int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
            if (fd >= 0) {
                dup2(fd, STDOUT_FILENO);
                dup2(fd, STDERR_FILENO);
                close(fd);
            }
        }
    }
    if (!iOSDetailedDiagnosticsEnabled()) return;
    task_vm_info_data_t memory = {};
    mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
    kern_return_t result = task_info(mach_task_self(), TASK_VM_INFO,
                                    reinterpret_cast<task_info_t>(&memory), &count);
    mach_task_basic_info_data_t basic = {};
    count = MACH_TASK_BASIC_INFO_COUNT;
    kern_return_t basicResult = task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                                        reinterpret_cast<task_info_t>(&basic), &count);
    task_thread_times_info_data_t times = {};
    count = TASK_THREAD_TIMES_INFO_COUNT;
    kern_return_t cpuResult = task_info(mach_task_self(), TASK_THREAD_TIMES_INFO,
                                      reinterpret_cast<task_info_t>(&times), &count);
    malloc_statistics_t allocations = {};
    malloc_zone_statistics(NULL, &allocations);
    char health[1024];
    snprintf(health, sizeof(health),
             "epoch=%ld context=%d input=%u vm_status=%d basic_status=%d cpu_status=%d footprint_bytes=%llu resident_bytes=%llu resident_peak_bytes=%llu virtual_bytes=%llu user_us=%llu system_us=%llu internal_bytes=%llu external_bytes=%llu compressed_bytes=%llu reusable_bytes=%llu malloc_inuse_bytes=%llu malloc_blocks=%llu",
             (long)time(NULL), s_gameContext.load(), s_inputState.load(), result, basicResult, cpuResult,
             (unsigned long long)memory.phys_footprint, (unsigned long long)memory.resident_size,
             (unsigned long long)basic.resident_size_max, (unsigned long long)basic.virtual_size,
             (unsigned long long)times.user_time.seconds * 1000000 + times.user_time.microseconds,
             (unsigned long long)times.system_time.seconds * 1000000 + times.system_time.microseconds,
             (unsigned long long)memory.internal, (unsigned long long)memory.external,
             (unsigned long long)memory.compressed, (unsigned long long)memory.reusable,
             (unsigned long long)allocations.size_in_use, (unsigned long long)allocations.blocks_in_use);
    iOSDiagnosticEvent("health", health);
    s_healthLog.Flush();

}

void iOSMissionDiagnostic(const char* phase, const char* mission)
{
    char detail[256];
    snprintf(detail, sizeof(detail), "epoch=%ld phase=%s mission=%s", (long)time(NULL), phase, mission);
    iOSDiagnosticEvent("mission", detail);
    iOSSampleRuntimeHealth();
}

void iOSLoadTiming(const char* phase, const char* file, unsigned waitMs, unsigned loadMs, int queued)
{
    char detail[512];
    snprintf(detail, sizeof(detail), "epoch=%ld phase=%s file=%s queue_wait_ms=%u load_ms=%u queued=%d",
             (long)time(NULL), phase, file, waitMs, loadMs, queued);
    iOSDiagnosticEvent("load-timing", detail);
}

static void TerminateHandler()
{
    void* frames[ 64 ];
    int n = backtrace( frames, 64 );
    char detail[192] = "no exception detail";
    std::exception_ptr eptr = std::current_exception();
    if( eptr != NULL ) {
        try { std::rethrow_exception( eptr ); }
        catch( const std::exception& e ) { snprintf(detail, sizeof(detail), "%s", e.what()); }
        catch( ... ) {}
    }

    char header[ 256 ];
    snprintf(header, sizeof(header), "std::terminate: %s", detail);
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
        stack_t stack = {};
        stack.ss_sp = s_signalStack;
        stack.ss_size = sizeof(s_signalStack);
        sigaltstack(&stack, NULL);
        struct sigaction action = {};
        action.sa_sigaction = SignalHandler;
        action.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_RESETHAND;
        sigemptyset(&action.sa_mask);
        const int signals[] = {SIGABRT, SIGSEGV, SIGBUS, SIGILL};
        for (unsigned i = 0; i < sizeof(signals) / sizeof(signals[0]); ++i)
            sigaction(signals[i], &action, NULL);
        std::set_terminate( TerminateHandler );
    }
}
#endif

#if defined(RAD_IOS)
// Main-thread aggregate: bounded diagnostic traffic, never a record per frame.
extern "C" void iOSFrameTimings(unsigned long long updateUs, unsigned long long renderUs,
                                unsigned long long fileUs, unsigned long long soundUs,
                                unsigned long long loadUs)
{
    static unsigned frames = 0;
    static unsigned long long sums[5] = {}, maxima[5] = {};
    static time_t reportTime = 0;
    const unsigned long long values[5] = {updateUs, renderUs, fileUs, soundUs, loadUs};
    ++frames;
    for (int i = 0; i < 5; ++i) {
        sums[i] += values[i];
        if (values[i] > maxima[i]) maxima[i] = values[i];
    }
    time_t now = time(NULL);
    if (!reportTime) reportTime = now;
    if (now == reportTime) return;
    char detail[768];
    snprintf(detail, sizeof(detail), "epoch=%ld context=%d frames=%u update_avg_us=%llu update_max_us=%llu render_avg_us=%llu render_max_us=%llu file_avg_us=%llu file_max_us=%llu sound_avg_us=%llu sound_max_us=%llu load_avg_us=%llu load_max_us=%llu",
             (long)now, s_gameContext.load(), frames,
             sums[0]/frames, maxima[0], sums[1]/frames, maxima[1], sums[2]/frames, maxima[2],
             sums[3]/frames, maxima[3], sums[4]/frames, maxima[4]);
    iOSDiagnosticEvent("frame-phases", detail);
    frames = 0;
    memset(sums, 0, sizeof(sums));
    memset(maxima, 0, sizeof(maxima));
    reportTime = now;
}
#endif
