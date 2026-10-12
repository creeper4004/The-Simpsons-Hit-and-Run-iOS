#ifndef IOS_GAME_DATA_REQUIREMENTS_H
#define IOS_GAME_DATA_REQUIREMENTS_H
#include <sys/stat.h>
#include <string>

// Shared by engine startup and the native importer. These are sentinel checks,
// not a full archive integrity or region compatibility check.
static const char* const IOS_REQUIRED_GAME_FILES[] = {
    "art/frontend/scrooby/bootup.p3d", "art/frontend/scrooby/ingame.p3d",
    "scripts/ss.mfk", "soundfx.rcf", "scripts.rcf", "dialog.rcf",
    "ambience.rcf", "carsound.rcf", "music00.rcf", "music01.rcf",
    "music02.rcf", "music03.rcf", "nis.rcf"
};
static inline bool IOSGameFilePresent(const char* root, const char* relative) {
    std::string path = std::string(root) + "/" + relative;
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode) && st.st_size > 0;
}
static inline bool IOSGameDataReady(const char* root) {
    for (const char* file : IOS_REQUIRED_GAME_FILES)
        if (!IOSGameFilePresent(root, file)) return false;
    return true;
}
#endif
