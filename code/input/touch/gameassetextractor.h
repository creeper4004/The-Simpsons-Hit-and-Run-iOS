#ifndef GAMEASSETEXTRACTOR_H_
#define GAMEASSETEXTRACTOR_H_

// iOS game data lives in Documents/game, independently of saves and logs.
// Reuse user-provided assets, migrate the legacy Documents tree by renaming,
// or extract a bundle only when the optional embedded build contains one.

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
    bool MigrateLegacyAssets();

    bool CopyKnownAssets();
    bool CopyAssetTree( const char* relativePath );
    bool CopyPathRecursive( const char* srcPath, const char* dstPath );

    bool EnsureDirectory( const char* path ) const;
    bool EnsureParentDirectoryForFile( const char* path ) const;
    bool FileExists( const char* path ) const;
    bool FileExistsAtOutput( const char* relativePath ) const;


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
