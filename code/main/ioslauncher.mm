#if defined(RAD_IOS)
#import <UIKit/UIKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#include <input/touch/iosgamedatarequirements.h>

extern "C" const char* iOSGetDocumentsDir(void);

static NSURL* GameURL() {
    return [NSURL fileURLWithPath:[@(iOSGetDocumentsDir()) stringByAppendingPathComponent:@"game"] isDirectory:YES];
}
static NSArray<NSString*>* MissingFiles(NSURL* root) {
    NSMutableArray* missing = [NSMutableArray array];
    for (const char* file : IOS_REQUIRED_GAME_FILES)
        if (!IOSGameFilePresent(root.path.fileSystemRepresentation, file)) [missing addObject:@(file)];
    return missing;
}
static void RecoverImport() {
    NSFileManager* fm = NSFileManager.defaultManager;
    [fm removeItemAtPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"srr-game-import"] error:nil];
    NSURL* backup = [GameURL().URLByDeletingLastPathComponent URLByAppendingPathComponent:@".game-import-backup"];
    if (![fm fileExistsAtPath:backup.path]) return;
    if (![fm fileExistsAtPath:GameURL().path]) [fm moveItemAtURL:backup toURL:GameURL() error:nil];
    else if (MissingFiles(GameURL()).count == 0) [fm removeItemAtURL:backup error:nil];
}

@interface SRRLauncher : UIViewController <UIDocumentPickerDelegate>
@property(nonatomic) BOOL finished;
@property(nonatomic) BOOL importing;
@property(atomic) BOOL cancelled;
@property(nonatomic,strong) UILabel* status;
@property(nonatomic,strong) UILabel* detail;
@property(nonatomic,strong) UIButton* play;
@property(nonatomic,strong) UIButton* importButton;
@property(nonatomic,strong) UIButton* checkButton;
@property(nonatomic,strong) UIActivityIndicatorView* activity;
@end

@implementation SRRLauncher
- (UILabel*)label:(NSString*)text style:(UIFontTextStyle)style {
    UILabel* label = [UILabel new]; label.text = text; label.numberOfLines = 0;
    label.font = [UIFont preferredFontForTextStyle:style];
    label.adjustsFontForContentSizeCategory = YES; label.textColor = UIColor.labelColor;
    return label;
}
- (UIButton*)button:(NSString*)title action:(SEL)action prominent:(BOOL)prominent {
    UIButton* b = [UIButton buttonWithType:UIButtonTypeSystem];
    UIButtonConfiguration* config = prominent ? UIButtonConfiguration.filledButtonConfiguration : UIButtonConfiguration.tintedButtonConfiguration;
    config.title = title; config.cornerStyle = UIButtonConfigurationCornerStyleLarge;
    config.contentInsets = NSDirectionalEdgeInsetsMake(14, 20, 14, 20);
    config.baseBackgroundColor = prominent ? UIColor.systemGreenColor : UIColor.secondarySystemBackgroundColor;
    config.baseForegroundColor = prominent ? UIColor.whiteColor : UIColor.labelColor;
    b.configuration = config; [b addTarget:self action:action forControlEvents:UIControlEventTouchUpInside];
    return b;
}
- (void)viewDidLoad {
    [super viewDidLoad]; self.view.backgroundColor = UIColor.systemBackgroundColor;
    UIScrollView* scroll = [UIScrollView new]; scroll.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:scroll];
    UIImageView* symbol = [[UIImageView alloc] initWithImage:[UIImage systemImageNamed:@"car.fill"]];
    symbol.tintColor = UIColor.systemGreenColor; symbol.contentMode = UIViewContentModeScaleAspectFit;
    [symbol.heightAnchor constraintEqualToConstant:40].active = YES;
    UILabel* title = [self label:@"Hit & Run" style:UIFontTextStyleLargeTitle];
    UILabel* subtitle = [self label:@"Your game. Ready to go." style:UIFontTextStyleTitle3];
#ifdef PAL
    NSString* edition = @"PC edition · PAL / EFIGS build";
#else
    NSString* edition = @"PC edition · North American build";
#endif
    UILabel* region = [self label:edition style:UIFontTextStyleFootnote]; region.textColor = UIColor.secondaryLabelColor;
    UILabel* note = [self label:@"Bring your own retail PC game files. No game data is included with this app." style:UIFontTextStyleBody];
    note.textColor = UIColor.secondaryLabelColor;
    UIButton* help = [self button:@"What files do I need?" action:@selector(help) prominent:NO];
    UIStackView* intro = [[UIStackView alloc] initWithArrangedSubviews:@[symbol,title,subtitle,region,note,help]];
    intro.axis = UILayoutConstraintAxisVertical; intro.spacing = 12; intro.alignment = UIStackViewAlignmentLeading;
    self.status = [self label:@"Set up your game" style:UIFontTextStyleTitle2];
    self.detail = [self label:@"" style:UIFontTextStyleBody]; self.detail.textColor = UIColor.secondaryLabelColor;
    self.importButton = [self button:@"Import game folder" action:@selector(importFolder) prominent:YES];
    self.checkButton = [self button:@"Check files" action:@selector(showFileCheck) prominent:NO];
    self.play = [self button:@"Play" action:@selector(startGame) prominent:YES];
    self.activity = [[UIActivityIndicatorView alloc] initWithActivityIndicatorStyle:UIActivityIndicatorViewStyleMedium];
    UIStackView* actions = [[UIStackView alloc] initWithArrangedSubviews:@[self.status,self.detail,self.activity,self.importButton,self.checkButton,self.play]];
    actions.axis = UILayoutConstraintAxisVertical; actions.spacing = 12;
    UIStackView* row = [[UIStackView alloc] initWithArrangedSubviews:@[intro,actions]];
    row.axis = UILayoutConstraintAxisHorizontal; row.spacing = 40; row.distribution = UIStackViewDistributionFillEqually;
    row.alignment = UIStackViewAlignmentCenter; row.translatesAutoresizingMaskIntoConstraints = NO;
    [scroll addSubview:row];
    UILayoutGuide* safe = self.view.safeAreaLayoutGuide;
    [NSLayoutConstraint activateConstraints:@[
        [scroll.leadingAnchor constraintEqualToAnchor:safe.leadingAnchor], [scroll.trailingAnchor constraintEqualToAnchor:safe.trailingAnchor],
        [scroll.topAnchor constraintEqualToAnchor:safe.topAnchor], [scroll.bottomAnchor constraintEqualToAnchor:safe.bottomAnchor],
        [row.leadingAnchor constraintEqualToAnchor:scroll.contentLayoutGuide.leadingAnchor constant:28],
        [row.trailingAnchor constraintEqualToAnchor:scroll.contentLayoutGuide.trailingAnchor constant:-28],
        [row.topAnchor constraintEqualToAnchor:scroll.contentLayoutGuide.topAnchor constant:24],
        [row.bottomAnchor constraintEqualToAnchor:scroll.contentLayoutGuide.bottomAnchor constant:-24],
        [row.widthAnchor constraintEqualToAnchor:scroll.frameLayoutGuide.widthAnchor constant:-56]
    ]];
    [self checkFiles];
}
- (void)setImportTitle:(NSString*)title {
    UIButtonConfiguration* config = self.importButton.configuration; config.title = title; self.importButton.configuration = config;
}
- (void)checkFiles {
    if (self.importing) return;
    RecoverImport();
    NSArray* missing = MissingFiles(GameURL()); BOOL ready = missing.count == 0;
    self.status.text = ready ? @"Ready to play" : @"Game files needed";
    self.detail.text = ready ? @"Startup files found. Your game data stays on this device." : @"Select the folder from your installed PC copy, or copy it into this app using Files or Finder.";
    self.play.enabled = ready;
    [self setImportTitle:ready ? @"Replace game files…" : @"Import game folder"];
}
- (void)showFileCheck {
    [self checkFiles];
    NSArray* missing = MissingFiles(GameURL());
    [self message:missing.count ? @"Missing or empty files" : @"Startup files found" text:missing.count ? [missing componentsJoinedByString:@"\n"] : @"Required startup files are present. This check does not verify every archive or determine region compatibility."];
}
- (void)message:(NSString*)title text:(NSString*)text {
    UIAlertController* alert = [UIAlertController alertControllerWithTitle:title message:text preferredStyle:UIAlertControllerStyleAlert];
    [alert addAction:[UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDefault handler:nil]];
    [self presentViewController:alert animated:YES completion:nil];
}
- (void)help {
    [self message:@"Use your PC game folder" text:@"Choose a folder containing art, movies, scripts, sound and the .rcf archives from your retail PC installation. Download cloud files first. Console ISOs and ZIP archives are not supported in this version.\n\nFiles/Finder transfers still work: put the contents in this app’s game folder. This build does not verify the data’s region or every archive’s integrity."];
}
- (void)chooseFolder {
    UIDocumentPickerViewController* picker = [[UIDocumentPickerViewController alloc] initForOpeningContentTypes:@[UTTypeFolder] asCopy:NO];
    picker.delegate = self; picker.allowsMultipleSelection = NO;
    [self presentViewController:picker animated:YES completion:nil];
}
- (void)importFolder {
    if (self.importing) { self.cancelled = YES; self.importButton.enabled = NO; return; }
    if (MissingFiles(GameURL()).count == 0) {
        UIAlertController* alert = [UIAlertController alertControllerWithTitle:@"Replace game files?" message:@"The new folder is checked before your current game data is replaced. Saves and settings are kept." preferredStyle:UIAlertControllerStyleAlert];
        [alert addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:nil]];
        [alert addAction:[UIAlertAction actionWithTitle:@"Choose folder" style:UIAlertActionStyleDefault handler:^(UIAlertAction* a) { [self chooseFolder]; }]];
        [self presentViewController:alert animated:YES completion:nil];
    } else [self chooseFolder];
}
- (void)documentPicker:(UIDocumentPickerViewController*)controller didPickDocumentsAtURLs:(NSArray<NSURL*>*)urls {
    NSURL* source = urls.firstObject; if (!source) return;
    self.importing = YES; self.cancelled = NO; self.play.enabled = NO; self.checkButton.enabled = NO;
    [self setImportTitle:@"Cancel import"];
    self.status.text = @"Importing game files"; self.detail.text = @"Keep the app open while files are copied."; [self.activity startAnimating];
    UIBackgroundTaskIdentifier task = [UIApplication.sharedApplication beginBackgroundTaskWithExpirationHandler:^{ self.cancelled = YES; }];
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED,0), ^{
        NSFileManager* fm = NSFileManager.defaultManager; BOOL scoped = [source startAccessingSecurityScopedResource];
        NSURL* stage = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"srr-game-import"] isDirectory:YES];
        __block NSError* error = nil;
        [fm createDirectoryAtURL:stage withIntermediateDirectories:YES attributes:nil error:&error];
        if (!error) {
            NSFileCoordinator* coordinator = [[NSFileCoordinator alloc] initWithFilePresenter:nil];
            NSError* coordinationError = nil;
            [coordinator coordinateReadingItemAtURL:source options:0 error:&coordinationError byAccessor:^(NSURL* accessible) {
                NSArray* missing = MissingFiles(accessible);
                if (missing.count) { error = [NSError errorWithDomain:@"GameImport" code:1 userInfo:@{NSLocalizedDescriptionKey:[@"Missing or empty startup files:\n" stringByAppendingString:[missing componentsJoinedByString:@"\n"]]}]; return; }
                NSArray* entries = [fm contentsOfDirectoryAtURL:accessible includingPropertiesForKeys:@[NSURLIsSymbolicLinkKey] options:NSDirectoryEnumerationSkipsHiddenFiles error:&error];
                for (NSURL* entry in entries) {
                    if (error || self.cancelled) break;
                    NSString* name = entry.lastPathComponent;
                    BOOL allowed = [@[@"art",@"movies",@"scripts",@"sound",@"simpsons.ini"] containsObject:name] || [name.pathExtension.lowercaseString isEqualToString:@"rcf"];
                    if (!allowed) continue;
                    dispatch_async(dispatch_get_main_queue(), ^{ self.detail.text = [@"Copying " stringByAppendingString:name]; });
                    // Reject symlinks throughout the selected data tree.
                    NSArray* tree = [fm subpathsOfDirectoryAtPath:entry.path error:nil] ?: @[];
                    NSMutableArray* paths = [NSMutableArray arrayWithObject:entry.path];
                    for (NSString* relative in tree) [paths addObject:[entry.path stringByAppendingPathComponent:relative]];
                    for (NSString* path in paths) {
                        NSDictionary* attrs = [fm attributesOfItemAtPath:path error:&error];
                        if (error) break;
                        if ([attrs[NSFileType] isEqual:NSFileTypeSymbolicLink]) { error = [NSError errorWithDomain:@"GameImport" code:2 userInfo:@{NSLocalizedDescriptionKey:@"Game folders containing symbolic links cannot be imported."}]; break; }
                    }
                    if (!error) [fm copyItemAtURL:entry toURL:[stage URLByAppendingPathComponent:name] error:&error];
                }
            }];
            if (!error) error = coordinationError;
        }
        if (!error && !self.cancelled && MissingFiles(stage).count) error = [NSError errorWithDomain:@"GameImport" code:3 userInfo:@{NSLocalizedDescriptionKey:@"Copied files are incomplete. Your existing game data was kept."}];
        BOOL installed = NO;
        if (!error && !self.cancelled) {
            dispatch_sync(dispatch_get_main_queue(), ^{ self.importButton.enabled = NO; self.status.text = @"Finishing import"; });
        }
        if (!error && !self.cancelled) {
            NSURL* target = GameURL(); NSURL* backup = [target.URLByDeletingLastPathComponent URLByAppendingPathComponent:@".game-import-backup"];
            // Do not overwrite a recovery backup left by an interrupted import.
            if ([fm fileExistsAtPath:backup.path]) error = [NSError errorWithDomain:@"GameImport" code:4 userInfo:@{NSLocalizedDescriptionKey:@"An earlier import needs recovery. Check files before trying again."}];
            BOOL hadOld = [fm fileExistsAtPath:target.path];
            if (!error && hadOld) [fm moveItemAtURL:target toURL:backup error:&error];
            if (!error) {
                [fm moveItemAtURL:stage toURL:target error:&error];
                if (error && hadOld) [fm moveItemAtURL:backup toURL:target error:nil];
                else if (!error) { installed = YES; if (hadOld) [fm removeItemAtURL:backup error:nil]; }
            }
        }
        [fm removeItemAtURL:stage error:nil]; if (scoped) [source stopAccessingSecurityScopedResource];
        dispatch_async(dispatch_get_main_queue(), ^{
            BOOL cancelled = self.cancelled && !installed; self.importing = NO; self.importButton.enabled = YES; self.checkButton.enabled = YES;
            [self.activity stopAnimating]; [self checkFiles];
            if (error) [self message:@"Import failed" text:error.localizedDescription];
            else if (cancelled) self.detail.text = @"Import cancelled. Existing game data was kept.";
            else self.detail.text = @"Import complete. Startup files checked; ready to play.";
            if (task != UIBackgroundTaskInvalid) [UIApplication.sharedApplication endBackgroundTask:task];
        });
    });
}
- (void)startGame {
    [self checkFiles]; if (!self.play.enabled) return;
    self.finished = YES;
}
@end

extern "C" void iOSShowGameLauncher() {
    // SDL calls SDL_main on the UI thread. Keep UIKit's run loop responsive
    // while the engine waits; no game assets or game-rendered UI are required.
    @autoreleasepool {
        UIWindow* previous = nil;
        for (UIScene* scene in UIApplication.sharedApplication.connectedScenes)
            if ([scene isKindOfClass:UIWindowScene.class])
                for (UIWindow* window in ((UIWindowScene*)scene).windows) if (window.isKeyWindow) previous = window;
        if (!previous) return;
        RecoverImport();
        UIWindow* window = [[UIWindow alloc] initWithWindowScene:previous.windowScene];
        SRRLauncher* launcher = [SRRLauncher new]; window.rootViewController = launcher;
        window.windowLevel = UIWindowLevelNormal + 1; [window makeKeyAndVisible];
        while (!launcher.finished) {
            @autoreleasepool { [[NSRunLoop mainRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.02]]; }
        }
        window.hidden = YES; [previous makeKeyAndVisible]; window.rootViewController = nil;
    }
}
#endif
