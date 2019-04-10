
// BasicOpenGLView.m - Outlaws.h platform view implementation for OSX

#import "BasicOpenGLView.h"
#include <execinfo.h>

#include "Outlaws.h"

#if OL_HAS_SDL
#include "../sdl_os/sdl_inc.h"
#endif

#include "posix.h"

BasicOpenGLView * gView = NULL;

static CFAbsoluteTime gStartTime = 0.0;


// ---------------------------------

double OL_GetCurrentTime()
{
    if (gStartTime == 0.0)
        gStartTime = CFAbsoluteTimeGetCurrent();
    return CFAbsoluteTimeGetCurrent () - gStartTime;
}

// copied from SDL_Delay
void OL_Sleep(double seconds)
{
    int was_error;
    struct timespec elapsed, tv;
    elapsed.tv_sec = floor(seconds);
    elapsed.tv_nsec = (seconds - elapsed.tv_sec) * 1e9;
    do {
        errno = 0;

        tv.tv_sec = elapsed.tv_sec;
        tv.tv_nsec = elapsed.tv_nsec;
        was_error = nanosleep(&tv, &elapsed);
    } while (was_error && (errno == EINTR));
}

// if error dump gl errors to debugger string, return error
GLenum glReportError (void)
{
    GLenum err = glGetError();
    if (GL_NO_ERROR != err)
        LogMessage([NSString stringWithFormat:@"GL Error: %s", (const char *) gluErrorString(err)]);
    return err;
}

void OL_GetWindowSize(float *pixelWidth, float *pixelHeight, float *pointWidth, float *pointHeight)
{
    NSRect pointSize = [gView bounds];
    NSRect pixelSize = [gView convertRectToBacking:pointSize];

    *pixelWidth = pixelSize.size.width;
    *pixelHeight = pixelSize.size.height;

    *pointWidth = pointSize.size.width;
    *pointHeight = pointSize.size.height;
}


static const NSApplicationPresentationOptions
kFullscreenPresentationOptions = (NSApplicationPresentationHideMenuBar|
                                  NSApplicationPresentationHideDock|
                                  NSApplicationPresentationFullScreen);

static void setupPresentationOptions(BOOL fullscreen)
{
    NSApplicationPresentationOptions opts = fullscreen ? kFullscreenPresentationOptions :
                                            NSApplicationPresentationDefault;
    [[NSApplication sharedApplication] setPresentationOptions:opts];
}

void OL_SetFullscreen(int fullscreen)
{
    fullscreen = fullscreen ? 1 : 0;
    if (fullscreen != gView->fullscreen)
    {
        gView->fullscreen = fullscreen;
        [[gView window] toggleFullScreen: gView];
        LogMessage(fullscreen ? @"SetFullscreen YES" : @"SetFullscreen NO");
     }
}

int OL_GetFullscreen(void)
{
    return gView->fullscreen;
}

void OL_SetWindowSizePoints(int w, int h)
{
    if (!gView || gView->fullscreen)
        return;
    NSRect frame = [[gView window] frame];
    if (frame.size.width == w && frame.size.height == h)
        return;
    // frame.origin.y += h - frame.size.height;
    frame.size = NSMakeSize(w, h);
    // [[gView window] setFrame:frame display:YES animate:NO];
    [[gView window] setContentMinSize: frame.size];
    [[gView window] setContentMaxSize: frame.size];
    [[gView window] setContentSize: frame.size];
    LogMessage([NSString stringWithFormat:@"Set window size to %dx%d", w, h]);
}

static bool hasScaleMethod()
{
    static BOOL checkedScaleMethod = NO;
    static BOOL hasScaleMethod     = NO;
    // this is really slow for some reason, so just do it once
    if (!checkedScaleMethod) {
        hasScaleMethod = [[NSScreen mainScreen] respondsToSelector:@selector(backingScaleFactor)];
        checkedScaleMethod = YES;
    }
    return hasScaleMethod;
}

float getBackingScaleFactor(void)
{
    CGFloat displayScale = 1.f;
    if (hasScaleMethod())
    {
        NSArray* screens = [NSScreen screens];
        for(NSScreen* screen in screens) {
            displayScale = MAX(displayScale, [screen backingScaleFactor]);
        }
    }
    
    return displayScale;
}

float OL_GetCurrentBackingScaleFactor(void)
{
    static CGFloat displayScale = 1.f;
    static int callIdx = 0;

    if (hasScaleMethod() && (callIdx++ % 100) == 0)
    {
        displayScale = [[[gView window] screen] backingScaleFactor];
    }

    static BOOL printedScreens = FALSE;
    if (!printedScreens)
    {
        printedScreens = TRUE;
        int i=1;
        NSArray* screens = [NSScreen screens];
        for (NSScreen* screen in screens)
        {
            const NSRect frame = [screen frame];
            const float scale = hasScaleMethod() ? [screen backingScaleFactor] : 1.f;
            LogMessage([NSString stringWithFormat:@"screen %d of %d is %dx%d pixels, scale of %g",
                                 i, (int) [screens count],
                                 (int)(frame.size.width * scale),
                                 (int)(frame.size.height * scale),
                                 scale]);
        }
    }
    
    return displayScale;
}

int OL_DoQuit(void)
{
    LogMessage(@"DoQuit");
    int wasClosing = gView->closing;
    gView->closing = 1;
    return wasClosing;
}

int OL_IsQuitting(void)
{
    return gView->closing;
}

#pragma mark ---- OpenGL Utils ----

// ===================================

@implementation BasicOpenGLView


// per-window timer function, basic time based animation preformed here
- (void)animationTimer:(NSTimer *)timer
{
    [self drawRect:[self bounds]]; // redraw now instead dirty to enable updates during live resize
}

static void doKeyEvent(enum EventType type, NSEvent *theEvent)
{
    NSString *characters = [theEvent charactersIgnoringModifiers];
    if ([characters length])
    {
        unichar character = [characters characterAtIndex:0];
        
        struct OLEvent e;
        memset(&e, 0, sizeof(e));

        // LogMessage([NSString stringWithFormat:@"Cocoa Key: '%C' %s",
                             // character, (type == OL_KEY_DOWN ? "Down" : "Up")]);

        switch (character)
        {
        case NSDeleteCharacter: e.key = NSBackspaceCharacter; break;
        case NSF13FunctionKey:  e.key = NSPrintScreenFunctionKey; break;
        case 25:                e.key = '\t'; break; // shift-tab
        default:                e.key = character; break;
        }

        e.type = type;
        OLG_OnEvent(&e);
    }
}

-(void)keyDown:(NSEvent *)theEvent
{
    doKeyEvent(OL_KEY_DOWN, theEvent);
}

-(void)keyUp:(NSEvent *)theEvent
{
    doKeyEvent(OL_KEY_UP, theEvent);
}

-(BOOL)performKeyEquivalent:(NSEvent *)theEvent
{
    // get events for command-S, etc.
#if 0
    NSString *characters = [theEvent charactersIgnoringModifiers];
    unichar character = [characters length] ? [characters characterAtIndex:0] : 0;
    if (character == 's')
    {
        doKeyEvent(OL_KEY_UP, theEvent);
        return YES;
    }
    else
#endif
    {
        return NO;
    }
}

static void modifierFlagToEvent(NSUInteger flags, NSUInteger lastFlags, NSUInteger mask, int eventKey)
{
    if ((flags&mask) ^ (lastFlags&mask))
    {
        struct OLEvent e;
        memset(&e, 0, sizeof(e));
        e.type = (flags&mask) ? OL_KEY_DOWN : OL_KEY_UP;
        e.key = eventKey;
        OLG_OnEvent(&e);
    }
}

- (void)flagsChanged:(NSEvent *)theEvent
{
    const NSUInteger flags = [theEvent modifierFlags];

    modifierFlagToEvent(flags, lastFlags, NSShiftKeyMask, OShiftKey);
    modifierFlagToEvent(flags, lastFlags, NSControlKeyMask, OControlKey);
    modifierFlagToEvent(flags, lastFlags, NSCommandKeyMask, OControlKey); // treat command as control
    modifierFlagToEvent(flags, lastFlags, NSAlternateKeyMask, OAltKey);

    self->lastFlags = flags;
}

// ---------------------------------

static int getKeyForMods(int key, int mods)
{
    if (key == 0 && (mods & NSControlKeyMask)) {
        return 1;
    } else if (key == 0 && (mods & NSCommandKeyMask)) {
        return 2;
    } else {
        return key;
    }
}

static void doMouseEvent(enum EventType type, NSEvent *theEvent)
{
    NSPoint location = [gView convertPoint:[theEvent locationInWindow] fromView:nil];
    
    struct OLEvent e;
    memset(&e, 0, sizeof(e));
    e.type = type;
    e.key = getKeyForMods([theEvent buttonNumber], [theEvent modifierFlags]);
    e.x = location.x;
    e.y = location.y;
    e.dx = [theEvent deltaX];
    e.dy = [theEvent deltaY];
    
    OLG_OnEvent(&e);
}

// ---------------------------------

- (void)mouseDown:(NSEvent *)theEvent
{
    doMouseEvent(OL_MOUSE_DOWN, theEvent);
}

- (void)rightMouseDown:(NSEvent *)theEvent
{
    doMouseEvent(OL_MOUSE_DOWN, theEvent);
}

- (void)otherMouseDown:(NSEvent *)theEvent
{
    doMouseEvent(OL_MOUSE_DOWN, theEvent);
}

- (void)mouseUp:(NSEvent *)theEvent
{
    doMouseEvent(OL_MOUSE_UP, theEvent);
}

- (void)rightMouseUp:(NSEvent *)theEvent
{
    doMouseEvent(OL_MOUSE_UP, theEvent);
}

- (void)otherMouseUp:(NSEvent *)theEvent
{
    doMouseEvent(OL_MOUSE_UP, theEvent);
}

- (void)mouseDragged:(NSEvent *)theEvent
{
    doMouseEvent(OL_MOUSE_DRAGGED, theEvent);
}

- (void)rightMouseDragged:(NSEvent *)theEvent
{
    doMouseEvent(OL_MOUSE_DRAGGED, theEvent);
}

- (void)otherMouseDragged:(NSEvent *)theEvent
{
    doMouseEvent(OL_MOUSE_DRAGGED, theEvent);
}

- (void)mouseMoved:(NSEvent *)theEvent
{
    doMouseEvent(OL_MOUSE_MOVED, theEvent);
}

// ---------------------------------

- (void)scrollWheel:(NSEvent *)theEvent
{
    struct OLEvent e;
    memset(&e, 0, sizeof(e));
    e.type = OL_SCROLL_WHEEL;
    e.dx = [theEvent deltaX];
    e.dy = [theEvent deltaY];

    // actually repect this! "natural" scrolling is "inverted" here.
    // BOOL inverted = [theEvent isDirectionInvertedFromDevice];
    // if (inverted) {
        // e.dy = -e.dy;
    // }
    
    OLG_OnEvent(&e);
}

// ---------------------------------


// from SDL2 SDL_cocoamouse.m
static NSCursor* invisibleCursor()
{
    static NSCursor *invisibleCursor = NULL;
    if (!invisibleCursor) {
        /* RAW 16x16 transparent GIF */
        static unsigned char cursorBytes[] = {
            0x47, 0x49, 0x46, 0x38, 0x37, 0x61, 0x10, 0x00, 0x10, 0x00, 0x80,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x21, 0xF9, 0x04,
            0x01, 0x00, 0x00, 0x01, 0x00, 0x2C, 0x00, 0x00, 0x00, 0x00, 0x10,
            0x00, 0x10, 0x00, 0x00, 0x02, 0x0E, 0x8C, 0x8F, 0xA9, 0xCB, 0xED,
            0x0F, 0xA3, 0x9C, 0xB4, 0xDA, 0x8B, 0xB3, 0x3E, 0x05, 0x00, 0x3B
        };

        NSData *cursorData = [NSData dataWithBytesNoCopy:&cursorBytes[0]
                                                  length:sizeof(cursorBytes)
                                            freeWhenDone:NO];
        NSImage *cursorImage = [[[NSImage alloc] initWithData:cursorData] autorelease];
        invisibleCursor = [[NSCursor alloc] initWithImage:cursorImage
                                                  hotSpot:NSZeroPoint];
    }

    return invisibleCursor;
}

- (void)cursorUpdate:(NSEvent *)event
{
    [invisibleCursor() set];
    LogMessage(@"cursorUpdate");
}

- (void)updateTrackingAreas
{
    [super updateTrackingAreas];
    [self removeTrackingArea:trackingArea];
    [trackingArea release];
    trackingArea = [[NSTrackingArea alloc] initWithRect:[self frame]
                                                options: (//NSTrackingMouseEnteredAndExited |
                                                    NSTrackingCursorUpdate|
                                                    // NSTrackingInVisibleRect|
                                                    NSTrackingMouseMoved |
                                                    NSTrackingActiveInActiveApp)
                                                  owner:self userInfo:nil];
    [self addTrackingArea:trackingArea];
}

// ---------------------------------

- (void) drawRect:(NSRect)rect
{    
    if (self->closing == 1) {
        self->closing = 2;
        [[self window] performClose: gView];
        LogMessage(@"performing close");
    } else {
#if OL_HAS_SDL
        SDL_Event evt;
        while (SDL_PollEvent(&evt))
            Controller_HandleEvent(&evt);
#endif
        
        NSRect rectView = [self convertRectToBacking:[self bounds]];
        glViewport(0, 0, rectView.size.width, rectView.size.height);

        static CGFloat width = 0;
        static CGFloat height = 0;

        if (width != rectView.size.width || height != rectView.size.height)
        {
            width = rectView.size.width;
            height = rectView.size.height;
            LogMessage([NSString stringWithFormat: @"Resized: %.fx%.f", width, height]);
        }
        
        // draw the whole game

        @try 
        {
            if (self->fullscreen && [NSCursor currentSystemCursor] != invisibleCursor())
            {
                [invisibleCursor() set];
                // LogMessage(@"cursor hack");
            }

            OLG_Draw();
        } 
        @catch (NSException* exception)
        {
            LogMessage([NSString stringWithFormat: @"Uncaught exception: %@\nStack trace: %@",
                                 exception.description, [exception callStackSymbols]]);
            posix_oncrash([[NSString stringWithFormat: @"Uncaught exception: %@",
                                     exception.description] UTF8String]);
        }
    }
}


void OL_WarpCursorPosition(float x, float y)
{
    NSRect window = NSMakeRect(x, [gView bounds].size.height - y,  0, 0);
    NSRect screen = [gView.window convertRectToScreen:window];
    CGWarpMouseCursorPosition(screen.origin);
}

void OL_GetCursorPosition(float *x, float *y)
{
    NSPoint coord = [gView.window mouseLocationOutsideOfEventStream];
    // coord.y = [gView bounds].size.height - coord.y;
    *x = coord.x;
    *y = coord.y;
}

void OL_Present()
{
    [[gView openGLContext] flushBuffer];
}

// ---------------------------------

void OL_SetSwapInterval(int interval)
{
    GLint swapInt = interval;
    [[gView openGLContext] setValues:&swapInt forParameter:NSOpenGLCPSwapInterval];
    LogMessage([NSString stringWithFormat:@"Set swap interval: %d", swapInt]);
}

// set initial OpenGL state (current context is set)
// called after context is created
- (void) prepareOpenGL
{
    [self setWantsBestResolutionOpenGLSurface:YES];

    OLG_InitGL(NULL);
}

// ---------------------------------

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (BOOL)becomeFirstResponder
{
    return  YES;
}

- (BOOL)resignFirstResponder
{
    return YES;
}

- (void)windowWillEnterFullScreen:(NSNotification *)notification
{
    LogMessage(@"Will Enter Fullscreen");
}

- (void)windowDidEnterFullScreen:(NSNotification *)notification
{
    self->fullscreen = YES;
    setupPresentationOptions(YES);
    // if (!self->closing)
        // OLG_SetFullscreenPref(1);
    LogMessage(@"Did Enter Fullscreen");
}

- (void)windowWillExitFullScreen:(NSNotification *)notification
{
    LogMessage(@"Will Exit Fullscreen");
}

- (void)windowDidExitFullScreen:(NSNotification *)notification
{
    self->fullscreen = NO;
    setupPresentationOptions(NO);
    
    // if (!self->closing)
        // OLG_SetFullscreenPref(0);
    LogMessage(@"Did Exit Fullscreen");
}

- (NSArray<NSWindow *> *)customWindowsToExitFullScreenForWindow:(NSWindow *)window
{
    return nil;
}

- (NSArray<NSWindow *> *)customWindowsToEnterFullScreenForWindow:(NSWindow *)window
{
    return nil;
}

- (void)windowDidFailToExitFullScreen:(NSWindow *)window
{
    LogMessage(@"Failed Exit Fullscreen");

    self->fullscreen = YES;
    
    //setupPresentationOptions(0);
    // OL_SetFullscreen(0);
}

- (void)windowDidFailToEnterFullScreen:(NSWindow *)window
{
    LogMessage(@"Failed Enter Fullscreen");
}

- (NSApplicationPresentationOptions)window:(NSWindow *)window
      willUseFullScreenPresentationOptions:(NSApplicationPresentationOptions)proposedOptions
{
    return kFullscreenPresentationOptions;
}

- (void)windowWillClose:(NSNotification *)notification
{
    LogMessage(@"window will close");
    OLG_OnQuit();
}

- (BOOL)windowShouldClose:(id)sender
{
    LogMessage(@"window should close");
    OLG_DoClose();
    return YES;
}

- (void) handleResignKeyNotification: (NSNotification *) notification
{
    struct OLEvent e;
    memset(&e, 0, sizeof(e));
    e.type = OL_LOST_FOCUS;
    OLG_OnEvent(&e);
}

- (void) handleBecomeKeyNotification: (NSNotification *) notification
{
    setupPresentationOptions(gView->fullscreen);
}

- (void) awakeFromNib
{
    NSOpenGLPixelFormatAttribute attrs[] =
        {
        NSOpenGLPFADoubleBuffer,
        NSOpenGLPFADepthSize, 24,
        NSOpenGLPFAOpenGLProfile,
        NSOpenGLProfileVersionLegacy,
        0
    };
    
    NSOpenGLPixelFormat *pf = [[[NSOpenGLPixelFormat alloc] initWithAttributes:attrs] autorelease];
    
    if (!pf)
    {
        NSLog(@"No OpenGL pixel format");
    }
    
    NSOpenGLContext* context = [[[NSOpenGLContext alloc] initWithFormat:pf shareContext:nil] autorelease];
    
    [self setPixelFormat:pf];
    [self setOpenGLContext:context];
    
    // start animation timer
    timer = [NSTimer timerWithTimeInterval:(1.0f/60.0f) target:self selector:@selector(animationTimer:) userInfo:nil repeats:YES];
    [[NSRunLoop currentRunLoop] addTimer:timer forMode:NSDefaultRunLoopMode];
    [[NSRunLoop currentRunLoop] addTimer:timer forMode:NSEventTrackingRunLoopMode]; // ensure timer fires during resize

    trackingArea = nil;
    lastFlags = 0;

    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(handleResignKeyNotification:)
               name:NSWindowDidResignKeyNotification
             object:nil ];
    
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(handleBecomeKeyNotification:)
               name:NSWindowDidBecomeKeyNotification
             object:nil ];

    

    gView = self;
    self->fullscreen = NO;
    self->closing = 0;

    [[self window] setDelegate:self];

#if OL_HAS_SDL
    Controller_Init();
#endif
}


@end
