#ifndef GAMEASSETEXTRACTOR_H_
#define GAMEASSETEXTRACTOR_H_

//=============================================================================
// GameAssetExtractor (iOS)
//
// The engine reads all game data (art/, movies/, scripts/, sound/, *.rcf,
// simpsons.ini) relative to the process working directory. On iOS the app is
// sandboxed: the bundle (Resources/) is read-only, and the only writable
// location the engine can chdir() to is the app's Documents/ directory.
//
// The Android port reads the game-asset tree directly from assets/ exposed in
// the app's files dir (CWD-relative). On iOS we instead bundle the whole
// tree inside the .app bundle and copy it out to Documents/ on first run,
// so the engine's CWD-relative file APIs resolve identically.
//
// Copies the bundled game assets from the app bundle Resource root (iOS only)
// into the writable Documents/ dir. Runs once per install (version-gated), or
// whenever a required top-level item is missing.
//=============================================================================

class GameAssetExtractor
{
public:
    static GameAssetExtractor& GetInstance();

    bool EnsureAssetsExtracted();

    const char* GetOutputRoot() const;
    const char* GetLastError() const;

private:
    GameAssetExtractor();
    ~GameAssetExtractor();

    // Not implemented.
    GameAssetExtractor( const GameAssetExtractor& );
    GameAssetExtractor& operator=( const GameAssetExtractor& );

    bool BuildOutputRoot();
    bool NeedsExtraction() const;

    bool CopyKnownAssets();
    bool CopyAssetTree( const char* relativePath );
    bool CopyPathRecursive( const char* srcPath, const char* dstPath );

    bool EnsureDirectory( const char* path ) const;
    bool EnsureParentDirectoryForFile( const char* path ) const;
    bool FileExists( const char* path ) const;
    bool FileExistsAtOutput( const char* relativePath ) const;

    bool WriteVersionFile();
    bool IsVersionFileValid() const;

    void SetLastError( const char* error );

private:
    enum
    {
        MAX_ASSET_PATH = 1024,
        MAX_ASSET_ERROR = 256,
        MAX_ASSET_ITEM_COUNT = 8
    };

    char mOutputRoot[ MAX_ASSET_PATH ];
    char mLastError[ MAX_ASSET_ERROR ];
    bool mHasRun;
};

#endif // GAMEASSETEXTRACTOR_H_
