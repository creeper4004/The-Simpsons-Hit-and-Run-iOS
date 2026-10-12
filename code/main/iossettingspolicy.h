#ifndef SRR_IOS_SETTINGS_POLICY_H
#define SRR_IOS_SETTINGS_POLICY_H
#include <cmath>
#include <cstring>

// 0 = automatic, 1 = always show, 2 = always hide.
inline bool IOSShowTouchControls(int policy, bool physicalController) {
    return policy == 1 || (policy != 2 && !physicalController);
}
inline float IOSSettingClamp(float value, float minimum, float maximum, float fallback) {
    return std::isfinite(value) ? (value < minimum ? minimum : value > maximum ? maximum : value) : fallback;
}
inline bool IOSBasicDiagnosticEvent(const char* event) {
    if (!event) return true;
    return strcmp(event,"health") && strcmp(event,"frame-stats") &&
        strcmp(event,"ui-frame") && strncmp(event,"pddi-",5) &&
        strcmp(event,"touch-context") && strcmp(event,"hud-map");
}
struct IOSFrameMeter {
    double start = -1;
    unsigned frames = 0;
    bool Add(double now, double& fps, double& milliseconds) {
        if (start < 0 || now < start) { start = now; frames = 0; return false; }
        ++frames;
        double elapsed = now - start;
        if (elapsed < 1.0) return false;
        fps = frames / elapsed; milliseconds = elapsed * 1000.0 / frames;
        start = now; frames = 0; return true;
    }
};
#endif
