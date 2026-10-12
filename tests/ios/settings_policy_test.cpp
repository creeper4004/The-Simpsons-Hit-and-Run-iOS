#include "../../code/main/iossettingspolicy.h"
#include "../../code/main/diagnostics/persistentlog.h"
#include <cassert>
#include <limits>
#include <cstdlib>
#include <fstream>
#include <string>
#include <cstdio>

int main() {
    assert(IOSShowTouchControls(0,false)); assert(!IOSShowTouchControls(0,true));
    assert(IOSShowTouchControls(1,false)); assert(IOSShowTouchControls(1,true));
    assert(!IOSShowTouchControls(2,false)); assert(!IOSShowTouchControls(2,true));
    assert(IOSSettingClamp(-1,0,1,.25)==0); assert(IOSSettingClamp(2,0,1,.25)==1);
    assert(IOSSettingClamp(std::numeric_limits<float>::quiet_NaN(),0,1,.25)==.25);
    assert(IOSBasicDiagnosticEvent("session-start")); assert(IOSBasicDiagnosticEvent("save-error"));
    assert(IOSBasicDiagnosticEvent("lifecycle")); assert(IOSBasicDiagnosticEvent("progress"));
    assert(!IOSBasicDiagnosticEvent("health")); assert(!IOSBasicDiagnosticEvent("frame-stats"));
    assert(!IOSBasicDiagnosticEvent("pddi-state")); assert(!IOSBasicDiagnosticEvent("ui-frame"));
    IOSFrameMeter meter; double fps=0,ms=0;
    assert(!meter.Add(0,fps,ms));
    for (int i=1;i<60;++i) assert(!meter.Add(i/60.0,fps,ms));
    assert(meter.Add(1,fps,ms)); assert(std::abs(fps-60)<.01); assert(std::abs(ms-1000.0/60)<.01);
    for (int i=1;i<30;++i) assert(!meter.Add(1+i/30.0,fps,ms));
    assert(meter.Add(2,fps,ms)); assert(std::abs(fps-30)<.01);
    char root[]="/tmp/shar-settings-log-XXXXXX"; assert(mkdtemp(root));
    {
        PersistentLog log; assert(log.Open(root,1024)); assert(log.Write("old","before clearing"));
        log.Clear(); assert(log.Write("new","after clearing")); log.Flush();
        std::ifstream file(std::string(root)+"/srr2_health.log"); std::string content((std::istreambuf_iterator<char>(file)),{});
        assert(content.find("before clearing")==std::string::npos); assert(content.find("after clearing")!=std::string::npos);
    }
    unlink((std::string(root)+"/srr2_health.log").c_str()); rmdir(root);
    puts("PASS: visibility policies, clamping, log levels, 30/60 FPS measurement and continued logging after clear");
}
