//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Corona game engine.
// For overview and more information on licensing please refer to README.md
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

// FTL Game Loop support for iOS template.
//
// This file is compiled as a standalone dylib and loaded into the pre-compiled
// template binary at runtime via LC_LOAD_DYLIB. It swizzles:
//
// 1. CADisplayLink.displayLinkWithTarget:selector: — replaces CADisplayLink
//    with an NSTimer-based shim, since CADisplayLink doesn't fire in the
//    headless FTL environment (no real display).
//
// 2. CoronaAppDelegate application:openURL:options: — handles the
//    firebase-game-loop:// URL scheme sent by Firebase Test Lab.
//
// No Corona C++ headers needed — pure ObjC runtime.
// No dependency on template binary symbols (they are stripped in release).

#import <objc/runtime.h>
#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

// ---------------------------------------------------------------------------
// CADisplayLink replacement: an NSTimer-backed shim
// ---------------------------------------------------------------------------

@interface _FTLTimerShim : NSObject
{
    NSTimer *_backingTimer;
    id _target;
    SEL _selector;
}
- (instancetype)initWithTarget:(id)target selector:(SEL)sel;
- (void)invalidate;
- (BOOL)isValid;
@end

@implementation _FTLTimerShim

- (instancetype)initWithTarget:(id)target selector:(SEL)sel
{
    self = [super init];
    if ( self )
    {
        _target = target;
        _selector = sel;

        // NSTimer retains its target (self), which keeps this shim alive.
        // Fires at ~60fps (16.67ms) to match the frame rate the Corona
        // runtime expects from CADisplayLink.
        _backingTimer = [NSTimer scheduledTimerWithTimeInterval:1.0/60.0
                            target:self
                            selector:@selector(timerFired:)
                            userInfo:nil
                            repeats:YES];
    }
    return self;
}

- (void)timerFired:(NSTimer *)timer
{
    // Forward to the original target (AppleTimer's fTarget/AppleCallback)
    // The invoke: method signature matches both CADisplayLink and NSTimer
    // callbacks (both pass self as argument, which is ignored).
    [_target performSelector:_selector withObject:timer];
}

- (void)setFrameInterval:(NSInteger)interval
{
    // No-op: our NSTimer uses a fixed 1/60s interval.
    // The original code does: NSInteger interval = (fInterval < 33 ? 1 : 2);
    // For FTL bench, 60fps is fine regardless of the requested interval.
}

- (void)addToRunLoop:(NSRunLoop *)runLoop forMode:(NSRunLoopMode)mode
{
    // No-op: the NSTimer created by scheduledTimerWithTimeInterval: is
    // already added to the current run loop's NSDefaultRunLoopMode.
    // We skip the CADisplayLink-specific run loop registration.
}

- (void)invalidate
{
    [_backingTimer invalidate];
    _backingTimer = nil;
    _target = nil;
}

- (BOOL)isValid
{
    return [_backingTimer isValid];
}

- (void)dealloc
{
    [self invalidate];
    [super dealloc];
}

@end

// ---------------------------------------------------------------------------
// Swizzled CADisplayLink factory
// ---------------------------------------------------------------------------

static id sSwizzledDisplayLink(id self, SEL _cmd, id target, SEL sel)
{
    return [[[_FTLTimerShim alloc] initWithTarget:target selector:sel] autorelease];
}

// ---------------------------------------------------------------------------
// Lazy CADisplayLink swizzler — safe to call before QuartzCore is loaded.
// ---------------------------------------------------------------------------

static bool sDisplayLinkSwizzled = false;

static void SwizzleDisplayLinkIfNeeded()
{
    if ( sDisplayLinkSwizzled ) { return; }
    Class dlClass = objc_getClass("CADisplayLink");
    if ( dlClass )
    {
        SEL dlSel = @selector(displayLinkWithTarget:selector:);
        Method dlMethod = class_getClassMethod(dlClass, dlSel);
        if ( dlMethod )
        {
            method_setImplementation(dlMethod, (IMP)sSwizzledDisplayLink);
            sDisplayLinkSwizzled = true;
        }
    }
}

// ---------------------------------------------------------------------------
// Swizzled CoronaAppDelegate openURL handler
// ---------------------------------------------------------------------------

static IMP sOriginalOpenURLOptions = NULL;

static BOOL sSwizzledOpenURLOptions(id self, SEL _cmd, UIApplication *app,
                                     NSURL *url, NSDictionary *options)
{
    // Retry CADisplayLink swizzle now that we're in application context
    // (QuartzCore is guaranteed to be loaded by this point).
    SwizzleDisplayLinkIfNeeded();

    NSString *scheme = [[url scheme] lowercaseString];
    if ( [scheme isEqualToString:@"firebase-game-loop"] )
    {
        NSString *scenario = [[url host] length] > 0 ? [url host] : @"1";

        // Store scenario number for main.lua to read
        [[NSUserDefaults standardUserDefaults] setObject:scenario
                                                  forKey:@"gameLoopScenario"];
        [[NSUserDefaults standardUserDefaults] synchronize];

        // Create game_loop_results directory
        NSArray *paths = NSSearchPathForDirectoriesInDomains(
            NSDocumentDirectory, NSUserDomainMask, YES);
        NSString *docsPath = [paths firstObject];
        NSString *glPath = [docsPath stringByAppendingPathComponent:
                            @"game_loop_results"];
        [[NSFileManager defaultManager] createDirectoryAtPath:glPath
                                  withIntermediateDirectories:YES
                                                   attributes:nil
                                                        error:nil];

        return YES;
    }

    // Not a Game Loop URL — forward to original handler
    if ( sOriginalOpenURLOptions )
    {
        return ((BOOL(*)(id, SEL, UIApplication *, NSURL *, NSDictionary *))
                sOriginalOpenURLOptions)(self, _cmd, app, url, options);
    }
    return NO;
}

// ---------------------------------------------------------------------------
// Constructor: swizzle at load time, before any Corona code runs
// ---------------------------------------------------------------------------

__attribute__((constructor))
static void FTLGameLoopInit()
{
    // 1. Try to swizzle CADisplayLink eagerly.
    //    May fail if QuartzCore hasn't been loaded yet (lazy framework).
    //    The openURL handler will retry if needed.
    SwizzleDisplayLinkIfNeeded();

    // 2. Swizzle CoronaAppDelegate's openURL handler.
    //    This intercepts the firebase-game-loop:// URL from FTL.
    //    It also retries the CADisplayLink swizzle, ensuring it works even
    //    if QuartzCore wasn't loaded at constructor time.
    Class adClass = objc_getClass("CoronaAppDelegate");
    if ( adClass )
    {
        SEL adSel = @selector(application:openURL:options:);
        Method adMethod = class_getInstanceMethod(adClass, adSel);
        if ( adMethod )
        {
            sOriginalOpenURLOptions = method_getImplementation(adMethod);
            method_setImplementation(adMethod, (IMP)sSwizzledOpenURLOptions);
        }
    }
}
