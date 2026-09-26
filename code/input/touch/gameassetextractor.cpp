//=============================================================================
// GameAssetExtractor.cpp  (RAD_IOS)
//
// The engine reads the whole game-asset tree CWD-relative. On iOS the app is
// sandboxed and chdir(Documents) is the only writable target. This extractor
// copies the bundled asset tree (from <bundle>/game_assets/) into Documents/
// on first run (or whenever the tree is incomplete), so pure3d / Scrooby /
// sound / scripts all resolve exactly as they do on Android where assets are
// exposed CWD-relative.
//
// Mirrors TouchAssetExtractor: version-gated, idempotent, writes a marker file.
// Uses POSIX dirent/stat/mkdir for a full recursive directory walk; no SDL
// directory API needed (avoids the SDL2 vs SDL3 directory-API mismatch).
//=============================================================================
#include "gameassetextractor.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef RAD_IOS
#include <SDL.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

#if SDL_MAJOR_VERSION < 3
#include <SDL_stdinc.h>   // SDL_free
#endif

extern "C" const char* iOSGetDocumentsDir(void);   // bridge (iosdocuments.mm)
extern "C" void iOSProgress(const char* stage);     // debug progress (win32main.cpp)

#define GAME_ASSET_VERSION_TAG "1"
#define GAME_ASSET_VERSION_FILE "game_assets.version"
#define GAME_ASSET_BUNDLE_ROOT "game_assets"

static const char* GAME_ASSET_ITEMS[] = {
    "art",
    "movies",
    "scripts",
    "sound",
    "ambience.rcf",
    "carsound.rcf",
    "dialog.rcf",
    "dialogs.rcf",
    "dialogf.rcf",
    "dialogg.rcf",
    "music00.rcf",
    "music01.rcf",
    "music02.rcf",
    "music03.rcf",
    "nis.rcf",
    "scripts.rcf",
    "soundfx.rcf",
    "simpsons.ini"
};

#endif // RAD_IOS
//=============================================================================
GameAssetExtractor& GameAssetExtractor::GetInstance()
{
    static GameAssetExtractor sInstance;
    return sInstance;
}

GameAssetExtractor::GameAssetExtractor()
{
    mOutputRoot[ 0 ] = '\0';
    mLastError[ 0 ] = '\0';
    mHasRun = false;
}

GameAssetExtractor::~GameAssetExtractor()
{
}

//=============================================================================
// EnsureAssetsExtracted
//=============================================================================
bool GameAssetExtractor::EnsureAssetsExtracted()
{
#if !defined(RAD_IOS)
    return true;
#else
    iOSProgress( "gameassets:enter" );

    if ( mHasRun && !NeedsExtraction() )
    {
        iOSProgress( "gameassets:early-ok" );
        return true;
    }

    if ( !BuildOutputRoot() )
    {
        iOSProgress( "gameassets:FAIL-BuildOutputRoot" );
        return false;
    }

    if ( !NeedsExtraction() )
    {
        mHasRun = true;
        iOSProgress( "gameassets:already-extracted" );
        return true;
    }

    iOSProgress( "gameassets:copy-begin" );
    if ( !CopyKnownAssets() )
    {
        iOSProgress( "gameassets:FAIL-CopyKnownAssets" );
        return false;
    }
    iOSProgress( "gameassets:copy-done" );

    if ( !WriteVersionFile() )
    {
        iOSProgress( "gameassets:FAIL-WriteVersionFile" );
        return false;
    }

    mHasRun = true;
    iOSProgress( "gameassets:done" );
    return true;
#endif
}

//=============================================================================
// GetOutputRoot
//=============================================================================
const char* GameAssetExtractor::GetOutputRoot() const
{
    return mOutputRoot;
}

//=============================================================================
// GetLastError
//=============================================================================
const char* GameAssetExtractor::GetLastError() const
{
    return mLastError[ 0 ] != '\0' ? mLastError : "none";
}

//=============================================================================
// BuildOutputRoot  -> <Documents>
//=============================================================================
bool GameAssetExtractor::BuildOutputRoot()
{
    const char* docs = iOSGetDocumentsDir();
    if ( docs == NULL || docs[ 0 ] == '\0' )
    {
        SetLastError( "iOSGetDocumentsDir returned null/empty." );
        return false;
    }

    snprintf( mOutputRoot, sizeof( mOutputRoot ), "%s", docs );

    if ( !EnsureDirectory( mOutputRoot ) )
    {
        SetLastError( "Documents dir is not accessible." );
        return false;
    }

    return true;
}

//=============================================================================
// NeedsExtraction
//=============================================================================
bool GameAssetExtractor::NeedsExtraction() const
{
    // If the version file matches and a couple of sentinel items exist, the
    // tree was already extracted. Otherwise re-extract.
    if ( IsVersionFileValid() )
    {
        for ( unsigned int i = 0; i < sizeof( GAME_ASSET_ITEMS ) / sizeof( GAME_ASSET_ITEMS[ 0 ] ); ++i )
        {
            if ( !FileExistsAtOutput( GAME_ASSET_ITEMS[ i ] ) )
            {
                return true;
            }
        }
        return false;
    }

    return true;
}

//=============================================================================
// CopyKnownAssets
//=============================================================================
bool GameAssetExtractor::CopyKnownAssets()
{
    for ( unsigned int i = 0; i < sizeof( GAME_ASSET_ITEMS ) / sizeof( GAME_ASSET_ITEMS[ 0 ] ); ++i )
    {
        if ( !CopyAssetTree( GAME_ASSET_ITEMS[ i ] ) )
        {
            return false;
        }
    }

    return true;
}

//=============================================================================
// CopyAssetTree  (recursive copy of one item / directory subtree)
//=============================================================================
bool GameAssetExtractor::CopyAssetTree( const char* relativePath )
{
    if ( relativePath == NULL || relativePath[ 0 ] == '\0' )
    {
        return true;
    }

    // Bundle source root.
    char bundleRoot[ MAX_ASSET_PATH ];
#if SDL_MAJOR_VERSION < 3
    const char* basePath = SDL_GetBasePath();
#else
    char* basePath = SDL_GetBasePath();
#endif
    const char* srcBase = ( basePath != NULL && basePath[ 0 ] != '\0' ) ? basePath : "";
    snprintf( bundleRoot, sizeof( bundleRoot ), "%s%s", srcBase, GAME_ASSET_BUNDLE_ROOT );
#if SDL_MAJOR_VERSION >= 3
    if ( basePath != NULL ) { SDL_free( basePath ); }
#endif

    char srcPath[ MAX_ASSET_PATH ];
    char dstPath[ MAX_ASSET_PATH ];
    snprintf( srcPath, sizeof( srcPath ), "%s/%s", bundleRoot, relativePath );
    snprintf( dstPath, sizeof( dstPath ), "%s/%s", mOutputRoot, relativePath );

    // If the source item does not exist in the bundle, skip (it may be an
    // optional per-language file e.g. dialogs.rcf). Not an error.
    if ( !FileExists( srcPath ) )
    {
        return true;
    }

    return CopyPathRecursive( srcPath, dstPath );
}

//=============================================================================
// CopyPathRecursive
//=============================================================================
bool GameAssetExtractor::CopyPathRecursive( const char* srcPath, const char* dstPath )
{
    struct stat st;
    if ( stat( srcPath, &st ) != 0 )
    {
        char err[ MAX_ASSET_ERROR ];
        snprintf( err, sizeof( err ), "stat failed: %s (%s)", srcPath, strerror( errno ) );
        SetLastError( err );
        return false;
    }

    if ( S_ISDIR( st.st_mode ) )
    {
        if ( !EnsureDirectory( dstPath ) )
        {
            char err[ MAX_ASSET_ERROR ];
            snprintf( err, sizeof( err ), "mkdir failed: %s (%s)", dstPath, strerror( errno ) );
            SetLastError( err );
            return false;
        }

        DIR* dir = opendir( srcPath );
        if ( dir == NULL )
        {
            char err[ MAX_ASSET_ERROR ];
            snprintf( err, sizeof( err ), "opendir failed: %s (%s)", srcPath, strerror( errno ) );
            SetLastError( err );
            return false;
        }

        struct dirent* entry;
        while ( ( entry = readdir( dir ) ) != NULL )
        {
            if ( strcmp( entry->d_name, "." ) == 0 || strcmp( entry->d_name, ".." ) == 0 )
            {
                continue;
            }

            char childSrc[ MAX_ASSET_PATH ];
            char childDst[ MAX_ASSET_PATH ];
            snprintf( childSrc, sizeof( childSrc ), "%s/%s", srcPath, entry->d_name );
            snprintf( childDst, sizeof( childDst ), "%s/%s", dstPath, entry->d_name );

            if ( !CopyPathRecursive( childSrc, childDst ) )
            {
                closedir( dir );
                return false;
            }
        }
        closedir( dir );
        return true;
    }

    if ( S_ISREG( st.st_mode ) )
    {
        if ( !EnsureParentDirectoryForFile( dstPath ) )
        {
            char err[ MAX_ASSET_ERROR ];
            snprintf( err, sizeof( err ), "parent mkdir failed: %s (%s)", dstPath, strerror( errno ) );
            SetLastError( err );
            return false;
        }

        FILE* in = fopen( srcPath, "rb" );
        if ( in == NULL )
        {
            char err[ MAX_ASSET_ERROR ];
            snprintf( err, sizeof( err ), "fopen src failed: %s (%s)", srcPath, strerror( errno ) );
            SetLastError( err );
            return false;
        }

        FILE* out = fopen( dstPath, "wb" );
        if ( out == NULL )
        {
            fclose( in );
            char err[ MAX_ASSET_ERROR ];
            snprintf( err, sizeof( err ), "fopen dst failed: %s (%s)", dstPath, strerror( errno ) );
            SetLastError( err );
            return false;
        }

        char buf[ 64 * 1024 ];
        size_t n;
        bool ok = true;
        while ( ( n = fread( buf, 1, sizeof( buf ), in ) ) > 0 )
        {
            if ( fwrite( buf, 1, n, out ) != n )
            {
                ok = false;
                break;
            }
        }

        fclose( in );
        if ( fclose( out ) != 0 )
        {
            ok = false;
        }

        if ( !ok )
        {
            SetLastError( "write failed during asset copy." );
            // Attempt to clean up the partial file.
            remove( dstPath );
            return false;
        }
        return true;
    }

    // Symlink / other: skip.
    return true;
}

//=============================================================================
// EnsureDirectory
//=============================================================================
bool GameAssetExtractor::EnsureDirectory( const char* path ) const
{
    if ( path == NULL || path[ 0 ] == '\0' )
    {
        return false;
    }

    struct stat st;
    if ( stat( path, &st ) == 0 )
    {
        return S_ISDIR( st.st_mode );
    }

    // Recursively create parents.
    char tmp[ MAX_ASSET_PATH ];
    snprintf( tmp, sizeof( tmp ), "%s", path );

    size_t len = strlen( tmp );
    if ( len == 0 )
    {
        return false;
    }

    // Build one component at a time.
    for ( size_t i = 1; i < len; ++i )
    {
        if ( tmp[ i ] == '/' )
        {
            tmp[ i ] = '\0';
            if ( mkdir( tmp, 0755 ) != 0 && errno != EEXIST )
            {
                return false;
            }
            tmp[ i ] = '/';
        }
    }

    if ( mkdir( tmp, 0755 ) != 0 && errno != EEXIST )
    {
        return false;
    }

    return true;
}

//=============================================================================
// EnsureParentDirectoryForFile
//=============================================================================
bool GameAssetExtractor::EnsureParentDirectoryForFile( const char* path ) const
{
    if ( path == NULL || path[ 0 ] == '\0' )
    {
        return false;
    }

    char tmp[ MAX_ASSET_PATH ];
    snprintf( tmp, sizeof( tmp ), "%s", path );

    char* slash = strrchr( tmp, '/' );
    if ( slash == NULL )
    {
        return true;   // no parent -> current dir
    }

    *slash = '\0';
    return EnsureDirectory( tmp );
}

//=============================================================================
// FileExists / FileExistsAtOutput
//=============================================================================
bool GameAssetExtractor::FileExists( const char* path ) const
{
    struct stat st;
    return path != NULL && stat( path, &st ) == 0;
}

bool GameAssetExtractor::FileExistsAtOutput( const char* relativePath ) const
{
    char full[ MAX_ASSET_PATH ];
    snprintf( full, sizeof( full ), "%s/%s", mOutputRoot, relativePath );
    return FileExists( full );
}

//=============================================================================
// WriteVersionFile / IsVersionFileValid
//=============================================================================
bool GameAssetExtractor::WriteVersionFile()
{
    char path[ MAX_ASSET_PATH ];
    snprintf( path, sizeof( path ), "%s/%s", mOutputRoot, GAME_ASSET_VERSION_FILE );

    FILE* f = fopen( path, "w" );
    if ( f == NULL )
    {
        SetLastError( "Cannot write game-assets version file." );
        return false;
    }
    fprintf( f, "%s\n", GAME_ASSET_VERSION_TAG );
    fclose( f );
    return true;
}

bool GameAssetExtractor::IsVersionFileValid() const
{
    char path[ MAX_ASSET_PATH ];
    snprintf( path, sizeof( path ), "%s/%s", mOutputRoot, GAME_ASSET_VERSION_FILE );

    FILE* f = fopen( path, "r" );
    if ( f == NULL )
    {
        return false;
    }

    char buf[ 32 ] = { 0 };
    if ( fgets( buf, sizeof( buf ), f ) == NULL )
    {
        fclose( f );
        return false;
    }
    fclose( f );

    return strncmp( buf, GAME_ASSET_VERSION_TAG "\n", sizeof( GAME_ASSET_VERSION_TAG ) + 1 ) == 0;
}

//=============================================================================
// SetLastError
//=============================================================================
void GameAssetExtractor::SetLastError( const char* error )
{
    if ( error == NULL )
    {
        mLastError[ 0 ] = '\0';
        return;
    }
    strncpy( mLastError, error, sizeof( mLastError ) - 1 );
    mLastError[ sizeof( mLastError ) - 1 ] = '\0';
}
