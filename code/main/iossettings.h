#ifndef SRR_IOS_SETTINGS_H
#define SRR_IOS_SETTINGS_H
#ifdef __cplusplus
extern "C" {
#endif
void iOSConfigureRuntimeSettings(void);
void iOSApplyInputSettings(void);
bool iOSDetailedDiagnosticsEnabled(void);
void iOSRecordGameFrame(void);
void iOSClearDiagnostics(void);
#ifdef __cplusplus
}
#endif
#ifdef __OBJC__
#import <UIKit/UIKit.h>
void iOSPresentSettings(UIViewController* presenter);
#endif
#endif
