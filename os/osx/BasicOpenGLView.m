
// BasicOpenGLView.m - Outlaws.h platform view implementation for OSX

#import "BasicOpenGLView.h"

#include "Outlaws.h"

// ==================================

// single set of interaction flags and states
BasicOpenGLView * gTrackingViewInfo = NULL;

// ==================================

#pragma mark ---- Utilities ----

static CFAbsoluteTime gStartTime = 0.0f;

// set app start time
static void setStartTime (void)
{
    gStartTime = CFAbsoluteTimeGetCurrent ();
}


// ---------------------------------

double OL_GetCurrentTime()
{
    return CFAbsoluteTimeGetCurrent () - gStartTime;
}

// if error dump gl errors to debugger string, return error
GLenum glReportError (void)
{
    GLenum err = glGetError();
    if (GL_NO_ERROR != err)
        OL_ReportMessage((const char *) gluErrorString(err));
    return err;
}

#pragma mark ---- Game Callbacks ----


void OL_GetWindowSize(float *pixelWidth, float *pixelHeight, float *pointWidth, float *pointHeight)
{
    NSRect pointSize = [gTrackingViewInfo bounds];
    NSRect pixelSize = [gTrackingViewInfo convertRectToBacking:pointSize];

    *pixelWidth = pixelSize.size.width;
    *pixelHeight = pixelSize.size.height;

    *pointWidth = pointSize.size.width;
    *pointHeight = pointSize.size.height;
}

void OL_SetFullscreen(int fullscreen)
{
    if (fullscreen != gTrackingViewInfo->fullscreen)
    {
        [[gTrackingViewInfo window] toggleFullScreen: gTrackingViewInfo];
        gTrackingViewInfo->fullscreen = fullscreen;
        OL_ReportMessage(fullscreen ? "[OSX] SetFullscreen YES" : "[OSX] SetFullscreen NO");
     }
}

int OL_GetFullscreen(void)
{
    return gTrackingViewInfo->fullscreen;
}

float OL_GetBackingScaleFactor()
{
    CGFloat displayScale = 1.0f;
    if (![[NSScreen mainScreen] respondsToSelector:@selector(backingScaleFactor)])
    {
        return displayScale;
    }

    NSArray* screens = [NSScreen screens];
    for(NSScreen* screen in screens) {
        displayScale = MAX(displayScale, [screen backingScaleFactor]);
    }
    return displayScale;
}

float OL_GetCurrentBackingScaleFactor(void)
{
    static CGFloat displayScale = 1.0f;
    static int callIdx = 0;

    // this is really slow for some reason, so just do it once
    static BOOL checkedScaleMethod = NO;
    static BOOL hasScaleMethod     = NO;
    if (!checkedScaleMethod) {
        hasScaleMethod = [[NSScreen mainScreen] respondsToSelector:@selector(backingScaleFactor)];
        checkedScaleMethod = YES;
    }

    if (hasScaleMethod && (callIdx++ % 100) == 0)
    {
        displayScale = [[[gTrackingViewInfo window] screen] backingScaleFactor];
    }
    
    return displayScale;
}

void OL_DoQuit(void)
{
    OL_ReportMessage("[OSX] DoQuit");
    gTrackingViewInfo->closing = 1;
}


#pragma mark ---- OpenGL Utils ----

// ===================================

@implementation BasicOpenGLView

// pixel format definition
+ (NSOpenGLPixelFormat*) basicPixelFormat
{
    /// WARNING WARNING this does not do anything!!!!!!!
    /// Use interface builder to update the pixel format...
    /// WARNING WARNING
    NSOpenGLPixelFormatAttribute attributes [] = {
        NSOpenGLPFAWindow,
        NSOpenGLPFADoubleBuffer,        // double buffered
        NSOpenGLPFADepthSize, (NSOpenGLPixelFormatAttribute)16, // 16 bit depth buffer

        NSOpenGLPFAMultisample,
        NSOpenGLPFASampleBuffers, (NSOpenGLPixelFormatAttribute)1,
        NSOpenGLPFASamples, (NSOpenGLPixelFormatAttribute)4,
        NSOpenGLPFANoRecovery,

        (NSOpenGLPixelFormatAttribute)nil
    };
    return [[[NSOpenGLPixelFormat alloc] initWithAttributes:attributes] autorelease];
}

// ---------------------------------

// per-window timer function, basic time based animation preformed here
- (void)animationTimer:(NSTimer *)timer
{
    time = CFAbsoluteTimeGetCurrent (); //reset time in all cases
    [self drawRect:[self bounds]]; // redraw now instead dirty to enable updates during live resize
}


#pragma mark ---- Method Overrides ----

-(void)keyDown:(NSEvent *)theEvent
{
    NSString *characters = [theEvent charactersIgnoringModifiers];
    if ([characters length])
    {
        unichar character = [characters characterAtIndex:0];
        
        struct OLEvent e;
        e.type = KEY_DOWN;
        e.key = character;
        OLG_OnEvent(&e);
    }
}

-(void)keyUp:(NSEvent *)theEvent
{
    NSString *characters = [theEvent charactersIgnoringModifiers];
    if ([characters length])
    {
        unichar character = [characters characterAtIndex:0];
        
        struct OLEvent e;
        e.type = KEY_UP;
        e.key = character;
        OLG_OnEvent(&e);
    }
}

static void modifierFlagToEvent(NSUInteger flags, NSUInteger lastFlags, NSUInteger mask, int eventKey)
{
    if ((flags&mask) ^ (lastFlags&mask))
    {
        struct OLEvent e;
        e.type = (flags&mask) ? KEY_DOWN : KEY_UP;
        e.key = eventKey;
        OLG_OnEvent(&e);
    }
}

- (void)flagsChanged:(NSEvent *)theEvent
{
    static NSUInteger lastFlags = 0;
    
    const NSUInteger flags = [theEvent modifierFlags];

    modifierFlagToEvent(flags, lastFlags, NSShiftKeyMask, OShiftKey);
    modifierFlagToEvent(flags, lastFlags, NSControlKeyMask, OControlKey);
    modifierFlagToEvent(flags, lastFlags, NSCommandKeyMask, OControlKey); // treat command as control
    modifierFlagToEvent(flags, lastFlags, NSAlternateKeyMask, OAltKey);

    lastFlags = flags;
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

- (void)mouseDown:(NSEvent *)theEvent
{
    NSPoint location = [self convertPoint:[theEvent locationInWindow] fromView:nil];
    
    struct OLEvent e;
    e.type = MOUSE_DOWN;
    e.key = getKeyForMods([theEvent buttonNumber], [theEvent modifierFlags]);
    e.x = location.x;
    e.y = [self bounds].size.height - location.y;
    OLG_OnEvent(&e);
}

// ---------------------------------

- (void)rightMouseDown:(NSEvent *)theEvent
{
    [self mouseDown:theEvent];
}

// ---------------------------------

- (void)otherMouseDown:(NSEvent *)theEvent
{
    [self mouseDown:theEvent];
}

// ---------------------------------

- (void)mouseUp:(NSEvent *)theEvent
{
    NSPoint location = [self convertPoint:[theEvent locationInWindow] fromView:nil];
    
    struct OLEvent e;
    e.type = MOUSE_UP;
    e.key = getKeyForMods([theEvent buttonNumber], [theEvent modifierFlags]);
    e.x = location.x;
    e.y = [self bounds].size.height - location.y;
    OLG_OnEvent(&e);
}

// ---------------------------------

- (void)rightMouseUp:(NSEvent *)theEvent
{
    [self mouseUp:theEvent];
}

// ---------------------------------

- (void)otherMouseUp:(NSEvent *)theEvent
{
    [self mouseUp:theEvent];
}

// ---------------------------------

- (void)mouseDragged:(NSEvent *)theEvent
{
    NSPoint location = [self convertPoint:[theEvent locationInWindow] fromView:nil];
    
    struct OLEvent e;
    e.type = MOUSE_DRAGGED;
    e.key = getKeyForMods([theEvent buttonNumber], [theEvent modifierFlags]);
    e.x = location.x;
    e.y = [self bounds].size.height - location.y;
    e.dx = [theEvent deltaX];
    e.dy = -[theEvent deltaY];
    OLG_OnEvent(&e);
}

// ---------------------------------

- (void)mouseMoved:(NSEvent *)theEvent
{
    if (!focused)
        CGDisplayHideCursor(kCGDirectMainDisplay);
    focused = true;

    NSPoint location = [self convertPoint:[theEvent locationInWindow] fromView:nil];
    
    struct OLEvent e;
    e.type = MOUSE_MOVED;
    e.key = getKeyForMods([theEvent buttonNumber], [theEvent modifierFlags]);
    e.x = location.x;
    e.y = [self bounds].size.height - location.y;
    e.dx = [theEvent deltaX];
    e.dy = -[theEvent deltaY];
    OLG_OnEvent(&e);
}

// ---------------------------------

- (void)scrollWheel:(NSEvent *)theEvent
{
    struct OLEvent e;
    e.type = SCROLL_WHEEL;
    e.dx = [theEvent deltaX];
    e.dy = [theEvent deltaY];

    BOOL inverted = [theEvent isDirectionInvertedFromDevice];
    if (inverted) {
        e.dy = -e.dy;
    }
    
    OLG_OnEvent(&e);
}

// ---------------------------------

- (void)rightMouseDragged:(NSEvent *)theEvent
{
    [self mouseDragged: theEvent];
}

// ---------------------------------

- (void)otherMouseDragged:(NSEvent *)theEvent
{
    [self mouseDragged: theEvent];
}

// ---------------------------------

- (void)mouseEntered:(NSEvent *)theEvent
{
    // hide the bitmap mouse cursor (we draw one with opengl)
    CGDisplayHideCursor(kCGDirectMainDisplay);
    focused = true;
}

// ---------------------------------

- (void)mouseExited:(NSEvent *)theEvent
{
    CGDisplayShowCursor(kCGDirectMainDisplay);
    focused = false;
}

// ---------------------------------

- (void)updateTrackingAreas 
{
    [self removeTrackingArea:trackingArea];
    [trackingArea release];
    trackingArea = [[NSTrackingArea alloc] initWithRect:[self frame]
        options: (NSTrackingMouseEnteredAndExited |
                  NSTrackingMouseMoved |
                  NSTrackingActiveInKeyWindow |
                  NSTrackingActiveAlways)
        owner:self userInfo:nil];
    [self addTrackingArea:trackingArea];
    [super updateTrackingAreas];
}

// ---------------------------------

- (void) drawRect:(NSRect)rect
{    
    if (self->closing == 1) {
        self->closing = 2;
        [[self window] performClose: gTrackingViewInfo];
    } else {
        NSRect rectView = [self convertRectToBacking:[self bounds]];
        glViewport (0, 0, rectView.size.width, rectView.size.height);
        // draw the whole game
        OLG_Draw();
    }
}

void OL_Present()
{
    [[gTrackingViewInfo openGLContext] flushBuffer];
}

// ---------------------------------

void OL_SetSwapInterval(int interval)
{
    GLint swapInt = interval;
    [[gTrackingViewInfo openGLContext] setValues:&swapInt forParameter:NSOpenGLCPSwapInterval];
}

// set initial OpenGL state (current context is set)
// called after context is created
- (void) prepareOpenGL
{
    [self setWantsBestResolutionOpenGLSurface:YES];
}
// ---------------------------------

// this can be a troublesome call to do anything heavyweight, as it is called on window moves, resizes, and display config changes.  So be
// careful of doing too much here.
- (void) update // window resizes, moves and display changes (resize, depth and display config change)
{
    [super update];
    if (![self inLiveResize])  {// if not doing live resize
        //[self updateInfoString]; // to get change in renderers will rebuld string every time (could test for early out)
        //getCurrentCaps (); // this call checks to see if the current config changed in a reasonably lightweight way to prevent expensive re-allocations
    }
}

// ---------------------------------

-(id) initWithFrame: (NSRect) frameRect
{
    NSOpenGLPixelFormat * pf = [BasicOpenGLView basicPixelFormat];

    self = [super initWithFrame: frameRect pixelFormat: pf];
    return self;
}

// ---------------------------------

- (BOOL)acceptsFirstResponder
{
    return YES;
}

// ---------------------------------

- (BOOL)becomeFirstResponder
{
    return  YES;
}

// ---------------------------------

- (BOOL)resignFirstResponder
{
    return YES;
}

// ---------------------------------

- (void)windowDidEnterFullScreen:(NSNotification *)notification
{
    self->fullscreen = YES;

    NSApplicationPresentationOptions opts = NSApplicationPresentationHideMenuBar|
                                            NSApplicationPresentationHideDock|
                                            NSApplicationPresentationFullScreen;
    [[NSApplication sharedApplication] setPresentationOptions:opts];

    if (!self->closing)
        OLG_SetFullscreenPref(1);
    OL_ReportMessage("[OSX] Will Enter Fullscreen");
}

- (void)windowDidExitFullScreen:(NSNotification *)notification
{
    self->fullscreen = NO;

    [[NSApplication sharedApplication] setPresentationOptions:NSApplicationPresentationDefault];

    if (!self->closing)
        OLG_SetFullscreenPref(0);
    OL_ReportMessage("[OSX] Will Exit Fullscreen");
}


- (void) handleResignKeyNotification: (NSNotification *) notification
{
    struct OLEvent e;
    e.type = LOST_FOCUS;
    OLG_OnEvent(&e);
}

- (void) awakeFromNib
{
    setStartTime (); // get app start time
    //getCurrentCaps (); // get current GL capabilites for all displays
        
    // set start values...
    time = CFAbsoluteTimeGetCurrent ();  // set animation time start time

    // start animation timer
    timer = [NSTimer timerWithTimeInterval:(1.0f/60.0f) target:self selector:@selector(animationTimer:) userInfo:nil repeats:YES];
    [[NSRunLoop currentRunLoop] addTimer:timer forMode:NSDefaultRunLoopMode];
    [[NSRunLoop currentRunLoop] addTimer:timer forMode:NSEventTrackingRunLoopMode]; // ensure timer fires during resize
    

    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(handleResignKeyNotification:)
               name:NSWindowDidResignKeyNotification
             object:nil ];
    
    gTrackingViewInfo = self;
    focused = false;
    self->fullscreen = NO;
    self->closing = 0;

    [[self window] setDelegate:self];
}


@end
