#include <input/touch/iosgamedatarequirements.h>
#include <cassert>
#include <fstream>
#include <cstdlib>
#include <unistd.h>
#include <cstdio>

int main() {
    char root[] = "/tmp/srr-game-data-XXXXXX";
    assert(mkdtemp(root));
    assert(!IOSGameDataReady(root));
    for (const char* file : IOS_REQUIRED_GAME_FILES) {
        std::string path = std::string(root) + "/" + file;
        for (size_t i = std::string(root).size() + 1; i < path.size(); ++i)
            if (path[i] == '/') mkdir(path.substr(0, i).c_str(), 0700);
        std::ofstream(path).put('x');
    }
    assert(IOSGameDataReady(root));
    std::string archive = std::string(root) + "/dialog.rcf";
    std::ofstream(archive, std::ios::trunc).close();
    assert(!IOSGameDataReady(root));
    unlink(archive.c_str()); mkdir(archive.c_str(), 0700);
    assert(!IOSGameDataReady(root));
    rmdir(archive.c_str()); std::ofstream(archive).put('x');
    assert(IOSGameDataReady(root));
    for (const char* file : IOS_REQUIRED_GAME_FILES)
        unlink((std::string(root) + "/" + file).c_str());
    const char* dirs[] = {"art/frontend/scrooby", "art/frontend", "art", "scripts", ""};
    for (const char* dir : dirs) rmdir((std::string(root) + "/" + dir).c_str());
    puts("PASS: missing, complete, empty and directory-instead-of-file data");
}
