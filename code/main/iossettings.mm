#if defined(RAD_IOS)
#import <UIKit/UIKit.h>
#include "iossettings.h"
#include <initializer_list>
#include "iossettingspolicy.h"
#include <input/touch/touchcontrolsconfigurationmanager.h>
#include <input/touch/touchcameracontroller.h>
#include <input/touch/touchinputmodemanager.h>
#include <atomic>
#include <cstdlib>

extern "C" const char* iOSGetDocumentsDir(void);
extern "C" void iOSDiagnosticFlush(void);
static std::atomic<bool> sDetailed(false);
static bool sShowFPS = false;
static NSString* const Opacity = @"ios.touch.opacity";
static NSString* const Pressed = @"ios.touch.pressedOpacity";
static NSString* const Visibility = @"ios.touch.visibility";
static NSString* const CameraX = @"ios.camera.horizontal";
static NSString* const CameraY = @"ios.camera.vertical";
static NSString* const InvertY = @"ios.camera.invertY";
static NSString* const FPS = @"ios.performance.fps";
static NSString* const Detailed = @"ios.diagnostics.detailed";
static NSString* Documents() { return @(iOSGetDocumentsDir()); }

static float StoredOpacity(NSString* key, NSString* fileKey, float fallback) {
    NSUserDefaults* defaults = NSUserDefaults.standardUserDefaults;
    if ([defaults objectForKey:key]) return IOSSettingClamp([defaults floatForKey:key],0,1,fallback);
    NSString* path = [Documents() stringByAppendingPathComponent:@"touch_controls/Simpsons_touch_controls_configuration.txt"];
    NSString* contents = [NSString stringWithContentsOfFile:path encoding:NSUTF8StringEncoding error:nil];
    for (NSString* line in [contents componentsSeparatedByCharactersInSet:NSCharacterSet.newlineCharacterSet]) {
        NSArray* parts = [line componentsSeparatedByString:@"="];
        if (parts.count == 2 && [[parts[0] stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceCharacterSet] isEqual:fileKey]) {
            float value; NSScanner* scanner = [NSScanner scannerWithString:parts[1]];
            if ([scanner scanFloat:&value]) return IOSSettingClamp(value,0,1,fallback);
        }
    }
    return fallback;
}
extern "C" bool iOSDetailedDiagnosticsEnabled() { return sDetailed.load(); }
extern "C" void iOSConfigureRuntimeSettings() {
    NSUserDefaults* defaults = NSUserDefaults.standardUserDefaults;
    [defaults registerDefaults:@{Visibility:@0, CameraX:@1.0, CameraY:@1.0, InvertY:@YES, FPS:@NO, Detailed:@NO}];
    bool detailed = [defaults boolForKey:Detailed]; sDetailed.store(detailed); sShowFPS = [defaults boolForKey:FPS];
    // These gates check variable presence, so disabled options must be unset.
    for (const char* key : {"IOS_RUNTIME_DIAG", "IOS_FRAME_STATS"}) {
        if (detailed) setenv(key,"1",1); else unsetenv(key);
    }
}
extern "C" void iOSApplyInputSettings() {
    NSUserDefaults* defaults = NSUserDefaults.standardUserDefaults;
    auto& config = TouchControlsConfigurationManager::GetInstance();
    bool changed = false;
    if ([defaults objectForKey:Opacity]) { config.SetOpacity(StoredOpacity(Opacity,@"opacity",0.25f)); changed = true; }
    if ([defaults objectForKey:Pressed]) { config.SetPressedOpacity(StoredOpacity(Pressed,@"pressed_opacity",0.85f)); changed = true; }
    if (changed && config.IsInitialized()) config.Save();
    TouchCameraController::GetInstance().SetSensitivity(
        200.0f * IOSSettingClamp([defaults floatForKey:CameraX],0.25f,3,1),
        25.0f * IOSSettingClamp([defaults floatForKey:CameraY],0.25f,3,1));
    TouchCameraController::GetInstance().SetInvertY([defaults boolForKey:InvertY]);
    int policy = (int)[defaults integerForKey:Visibility];
    TouchInputModeManager::GetInstance().SetIosTouchVisibility(policy >= 0 && policy <= 2 ? policy : 0);
}
extern "C" void iOSRecordGameFrame() {
    if (!sShowFPS) return;
    static IOSFrameMeter meter;
    double fps, milliseconds;
    if (!meter.Add(NSProcessInfo.processInfo.systemUptime, fps, milliseconds)) return;
    static UILabel* label;
    if (!label) {
        UIWindow* window = nil;
        for (UIScene* scene in UIApplication.sharedApplication.connectedScenes)
            if ([scene isKindOfClass:UIWindowScene.class])
                for (UIWindow* candidate in ((UIWindowScene*)scene).windows) if (candidate.isKeyWindow) window = candidate;
        if (!window) return;
        label = [UILabel new]; label.translatesAutoresizingMaskIntoConstraints = NO;
        label.font = [UIFont monospacedDigitSystemFontOfSize:12 weight:UIFontWeightMedium];
        label.textColor = UIColor.whiteColor; label.backgroundColor = [UIColor.blackColor colorWithAlphaComponent:0.65];
        label.layer.cornerRadius = 6; label.clipsToBounds = YES; label.userInteractionEnabled = NO;
        label.accessibilityLabel = @"Frame rate and average frame interval";
        [window addSubview:label];
        [NSLayoutConstraint activateConstraints:@[
            [label.topAnchor constraintEqualToAnchor:window.safeAreaLayoutGuide.topAnchor constant:6],
            [label.trailingAnchor constraintEqualToAnchor:window.safeAreaLayoutGuide.trailingAnchor constant:-12],
            [label.heightAnchor constraintEqualToConstant:28]
        ]];
    }
    label.text = [NSString stringWithFormat:@"  %.0f FPS · %.1f ms  ",fps,milliseconds];
}

@interface SRRSettings : UITableViewController
@end
@implementation SRRSettings
- (void)viewDidLoad {
    [super viewDidLoad]; self.title = @"Settings";
    self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemDone target:self action:@selector(done)];
    self.tableView.rowHeight = 72;
}
- (void)done { iOSConfigureRuntimeSettings(); [self dismissViewControllerAnimated:YES completion:nil]; }
- (NSInteger)numberOfSectionsInTableView:(UITableView*)table { return 5; }
- (NSInteger)tableView:(UITableView*)table numberOfRowsInSection:(NSInteger)section { return (NSInteger[]){3,3,1,3,2}[section]; }
- (NSString*)tableView:(UITableView*)table titleForHeaderInSection:(NSInteger)section { return @[@"Touch controls",@"Camera",@"Performance",@"Diagnostics",@"Saves and defaults"][section]; }
- (NSString*)tableView:(UITableView*)table titleForFooterInSection:(NSInteger)section {
    return @[@"Opacity changes appearance. Always hide disables touch controls and requires a controller.",
        @"Camera sensitivity is relative to the original defaults. Settings apply when you select Play.",
        @"Shows game frame submissions and average frame interval, not GPU execution time.",
        @"Basic keeps crashes, errors and important events. Detailed adds performance samples and bounded traces. Review exported logs before sharing.",
        @"Backups copy existing save slots into Files → Simpsons Hit & Run → Save Backups. Game files are excluded. Defaults reset these settings only; your saves and button layouts are kept."][section];
}
- (void)addSlider:(UITableViewCell*)cell key:(NSString*)key value:(float)value minimum:(float)minimum maximum:(float)maximum {
    UISlider* slider = [[UISlider alloc] initWithFrame:CGRectMake(0,0,180,44)];
    slider.minimumValue = minimum; slider.maximumValue = maximum; slider.value = value;
    slider.accessibilityIdentifier = key; slider.accessibilityLabel = cell.textLabel.text;
    slider.accessibilityValue = [NSString stringWithFormat:@"%.0f percent",value * 100];
    [slider addTarget:self action:@selector(sliderChanged:) forControlEvents:UIControlEventValueChanged];
    cell.accessoryView = slider; cell.selectionStyle = UITableViewCellSelectionStyleNone;
    cell.detailTextLabel.text = [NSString stringWithFormat:@"%.0f%%",value * 100];
}
- (void)addSwitch:(UITableViewCell*)cell key:(NSString*)key {
    UISwitch* toggle = [UISwitch new]; toggle.on = [NSUserDefaults.standardUserDefaults boolForKey:key];
    toggle.accessibilityIdentifier = key; toggle.accessibilityLabel = cell.textLabel.text;
    [toggle addTarget:self action:@selector(switchChanged:) forControlEvents:UIControlEventValueChanged]; cell.accessoryView = toggle;
    cell.selectionStyle = UITableViewCellSelectionStyleNone;
}
- (UITableViewCell*)tableView:(UITableView*)table cellForRowAtIndexPath:(NSIndexPath*)index {
    UITableViewCell* cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleSubtitle reuseIdentifier:nil];
    cell.textLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    cell.textLabel.adjustsFontForContentSizeCategory = YES; cell.detailTextLabel.textColor = UIColor.secondaryLabelColor;
    NSInteger section = index.section, row = index.row;
    NSUserDefaults* defaults = NSUserDefaults.standardUserDefaults;
    if (section == 0 && row < 2) {
        cell.textLabel.text = row == 0 ? @"Button opacity" : @"Pressed opacity";
        [self addSlider:cell key:row == 0 ? Opacity : Pressed value:StoredOpacity(row == 0 ? Opacity : Pressed,row == 0 ? @"opacity" : @"pressed_opacity",row == 0 ? 0.25f : 0.85f) minimum:0 maximum:1];
    } else if (section == 0) {
        cell.textLabel.text = @"Controller visibility";
        UISegmentedControl* control = [[UISegmentedControl alloc] initWithItems:@[@"Auto",@"Show",@"Hide"]];
        control.selectedSegmentIndex = [defaults integerForKey:Visibility]; control.accessibilityLabel = @"Touch control visibility";
        [control addTarget:self action:@selector(visibilityChanged:) forControlEvents:UIControlEventValueChanged]; cell.accessoryView = control;
        cell.selectionStyle = UITableViewCellSelectionStyleNone;
    } else if (section == 1 && row < 2) {
        cell.textLabel.text = row == 0 ? @"Horizontal sensitivity" : @"Vertical sensitivity";
        [self addSlider:cell key:row == 0 ? CameraX : CameraY value:[defaults floatForKey:row == 0 ? CameraX : CameraY] minimum:0.25f maximum:3];
    } else if (section == 1) {
        cell.textLabel.text = @"Invert camera Y"; [self addSwitch:cell key:InvertY];
    } else if (section == 2) {
        cell.textLabel.text = @"Show FPS counter"; [self addSwitch:cell key:FPS];
    } else if (section == 3 && row == 0) {
        cell.textLabel.text = @"Detailed logging"; [self addSwitch:cell key:Detailed];
        cell.detailTextLabel.text = [defaults boolForKey:Detailed] ? @"Detailed" : @"Basic";
    } else {
        cell.textLabel.text = section == 3 ? (row == 1 ? @"Share diagnostic report" : @"Clear logs…") : (row == 0 ? @"Back up and export saves" : @"Reset settings…");
        cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
    }
    return cell;
}
- (void)sliderChanged:(UISlider*)slider {
    [NSUserDefaults.standardUserDefaults setFloat:slider.value forKey:slider.accessibilityIdentifier];
    slider.accessibilityValue = [NSString stringWithFormat:@"%.0f percent",slider.value * 100];
    UIView* view = slider; while (view && ![view isKindOfClass:UITableViewCell.class]) view = view.superview;
    ((UITableViewCell*)view).detailTextLabel.text = [NSString stringWithFormat:@"%.0f%%",slider.value * 100];
}
- (void)switchChanged:(UISwitch*)toggle {
    [NSUserDefaults.standardUserDefaults setBool:toggle.on forKey:toggle.accessibilityIdentifier];
    if ([toggle.accessibilityIdentifier isEqual:Detailed]) { iOSConfigureRuntimeSettings(); [self.tableView reloadData]; }
}
- (void)visibilityChanged:(UISegmentedControl*)control { [NSUserDefaults.standardUserDefaults setInteger:control.selectedSegmentIndex forKey:Visibility]; }
- (void)alert:(NSString*)title message:(NSString*)message {
    UIAlertController* alert = [UIAlertController alertControllerWithTitle:title message:message preferredStyle:UIAlertControllerStyleAlert];
    [alert addAction:[UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDefault handler:nil]];
    [self presentViewController:alert animated:YES completion:nil];
}
- (void)confirm:(NSString*)title message:(NSString*)message action:(void (^)(void))action {
    UIAlertController* alert = [UIAlertController alertControllerWithTitle:title message:message preferredStyle:UIAlertControllerStyleAlert];
    [alert addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:nil]];
    [alert addAction:[UIAlertAction actionWithTitle:@"Confirm" style:UIAlertActionStyleDestructive handler:^(UIAlertAction* a) { action(); }]];
    [self presentViewController:alert animated:YES completion:nil];
}
- (void)share:(NSArray<NSURL*>*)files {
    UIActivityViewController* sheet = [[UIActivityViewController alloc] initWithActivityItems:files applicationActivities:nil];
    sheet.popoverPresentationController.sourceView = self.view;
    sheet.popoverPresentationController.sourceRect = CGRectMake(self.view.bounds.size.width/2,self.view.bounds.size.height/2,1,1);
    [self presentViewController:sheet animated:YES completion:nil];
}
- (void)exportLogs {
    iOSDiagnosticFlush();
    NSMutableData* report = [NSMutableData data];
    NSString* header = [NSString stringWithFormat:@"Hit & Run %@ · iOS %@\n\n",[NSBundle.mainBundle objectForInfoDictionaryKey:@"CFBundleShortVersionString"],UIDevice.currentDevice.systemVersion];
    [report appendData:[header dataUsingEncoding:NSUTF8StringEncoding]];
    for (NSString* name in @[@"srr2_health.log",@"srr2_health.previous.log",@"srr2_console.log",@"srr2_console.previous.log",@"srr2_crash.txt",@"srr2_runtime_diag.txt"]) {
        NSFileHandle* file = [NSFileHandle fileHandleForReadingAtPath:[Documents() stringByAppendingPathComponent:name]];
        if (!file) continue;
        unsigned long long length = [file seekToEndOfFile];
        [file seekToFileOffset:length > 2*1024*1024 ? length - 2*1024*1024 : 0];
        [report appendData:[[NSString stringWithFormat:@"\n--- %@ (last 2 MiB maximum) ---\n",name] dataUsingEncoding:NSUTF8StringEncoding]];
        [report appendData:[file readDataToEndOfFile]]; [file closeFile];
    }
    NSURL* output = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:[NSString stringWithFormat:@"Hit-and-Run-Diagnostics-%@.txt",NSUUID.UUID.UUIDString]]];
    NSError* error = nil;
    if (![report writeToURL:output options:NSDataWritingAtomic error:&error]) [self alert:@"Export failed" message:error.localizedDescription];
    else [self share:@[output]];
}
- (void)backupSaves {
    NSFileManager* fm = NSFileManager.defaultManager;
    NSMutableArray<NSString*>* slots = [NSMutableArray array];
    for (int i=1;i<=8;++i) {
        NSString* name = [NSString stringWithFormat:@"Save%d",i];
        NSDictionary* attrs = [fm attributesOfItemAtPath:[Documents() stringByAppendingPathComponent:name] error:nil];
        if ([attrs[NSFileType] isEqual:NSFileTypeRegular] && [attrs[NSFileSize] unsignedLongLongValue] > 0) [slots addObject:name];
    }
    if (!slots.count) { [self alert:@"No saves found" message:@"Save your progress in-game first, then reopen the app to back up your save slots."]; return; }
    NSDateFormatter* formatter = [NSDateFormatter new]; formatter.locale = [NSLocale localeWithLocaleIdentifier:@"en_US_POSIX"]; formatter.dateFormat = @"yyyy-MM-dd_HHmmss";
    NSString* name = [NSString stringWithFormat:@"%@_%@",[formatter stringFromDate:NSDate.date],[NSUUID.UUID.UUIDString substringToIndex:8]];
    NSURL* folder = [NSURL fileURLWithPath:[[Documents() stringByAppendingPathComponent:@"Save Backups"] stringByAppendingPathComponent:name] isDirectory:YES];
    NSError* error = nil; [fm createDirectoryAtURL:folder withIntermediateDirectories:YES attributes:nil error:&error];
    NSMutableArray* files = [NSMutableArray array];
    for (NSString* slot in slots) {
        if (error) break;
        NSURL* output = [folder URLByAppendingPathComponent:slot];
        if ([fm copyItemAtPath:[Documents() stringByAppendingPathComponent:slot] toPath:output.path error:&error]) [files addObject:output];
    }
    if (error) { [fm removeItemAtURL:folder error:nil]; [self alert:@"Backup failed" message:error.localizedDescription]; }
    else [self share:files];
}
- (void)tableView:(UITableView*)table didSelectRowAtIndexPath:(NSIndexPath*)index {
    [table deselectRowAtIndexPath:index animated:YES];
    if (index.section == 3 && index.row == 1) [self exportLogs];
    else if (index.section == 3 && index.row == 2) [self confirm:@"Clear diagnostic logs?" message:@"Existing diagnostic records will be removed. Game files and saves are kept." action:^{ iOSClearDiagnostics(); }];
    else if (index.section == 4 && index.row == 0) [self backupSaves];
    else if (index.section == 4 && index.row == 1) [self confirm:@"Reset settings?" message:@"Restore these controls, camera and diagnostic settings to their defaults. Button layouts and saves are kept." action:^{
        NSUserDefaults* defaults = NSUserDefaults.standardUserDefaults;
        [defaults setFloat:0.25f forKey:Opacity]; [defaults setFloat:0.85f forKey:Pressed];
        for (NSString* key in @[Visibility,CameraX,CameraY,InvertY,FPS,Detailed]) [defaults removeObjectForKey:key];
        iOSConfigureRuntimeSettings(); [self.tableView reloadData];
    }];
}
@end
void iOSPresentSettings(UIViewController* presenter) {
    SRRSettings* settings = [[SRRSettings alloc] initWithStyle:UITableViewStyleInsetGrouped];
    UINavigationController* nav = [[UINavigationController alloc] initWithRootViewController:settings];
    nav.modalPresentationStyle = UIModalPresentationFullScreen;
    [presenter presentViewController:nav animated:YES completion:nil];
}
#endif
