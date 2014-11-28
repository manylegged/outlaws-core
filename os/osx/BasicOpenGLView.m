
// BasicOpenGLView.m - Outlaws.h platform view implementation for OSX

#import "BasicOpenGLView.h"
#include <sys/signal.h>

#include "sdl_inc.h"
#include "Outlaws.h"

BasicOpenGLView * gView = NULL;

static CFAbsoluteTime gStartTime = 0.0;


// ---------------------------------

double OL_GetCurrentTime()
{
    if (gStartTime == 0.0)
        gStartTime = CFAbsoluteTimeGetCurrent();
    return CFAbsoluteTimeGetCurrent () - gStartTime;
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

void OL_SetFullscreen(int fullscreen)
{
    fullscreen = fullscreen ? 1 : 0;
    if (fullscreen != gView->fullscreen)
    {
        [[gView window] toggleFullScreen: gView];
        gView->fullscreen = fullscreen;
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

float OL_GetBackingScaleFactor()
{
    CGFloat displayScale = 1.f;
    if (hasScaleMethod())
    {
        NSArray* screens = [NSScreen screens];
        for(NSScreen* screen in screens) {
            displayScale = MAX(displayScale, [screen backingScaleFactor]);
        }
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

float OL_GetCurrentBackingScaleFactor(void)
{
    static CGFloat displayScale = 1.f;
    static int callIdx = 0;

    if (hasScaleMethod() && (callIdx++ % 100) == 0)
    {
        displayScale = [[[gView window] screen] backingScaleFactor];
    }
    
    return displayScale;
}

void OL_DoQuit(void)
{
    LogMessage(@"DoQuit");
    gView->closing = 1;
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
    doKeyEvent(KEY_DOWN, theEvent);
}

-(void)keyUp:(NSEvent *)theEvent
{
    doKeyEvent(KEY_UP, theEvent);
}

static void modifierFlagToEvent(NSUInteger flags, NSUInteger lastFlags, NSUInteger mask, int eventKey)
{
    if ((flags&mask) ^ (lastFlags&mask))
    {
        struct OLEvent e;
        memset(&e, 0, sizeof(e));
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
    doMouseEvent(MOUSE_DOWN, theEvent);
}

- (void)rightMouseDown:(NSEvent *)theEvent
{
    [self mouseDown:theEvent];
}

- (void)otherMouseDown:(NSEvent *)theEvent
{
    [self mouseDown:theEvent];
}

- (void)mouseUp:(NSEvent *)theEvent
{
    doMouseEvent(MOUSE_UP, theEvent);
}

- (void)rightMouseUp:(NSEvent *)theEvent
{
    [self mouseUp:theEvent];
}

- (void)otherMouseUp:(NSEvent *)theEvent
{
    [self mouseUp:theEvent];
}

- (void)mouseDragged:(NSEvent *)theEvent
{
    doMouseEvent(MOUSE_DRAGGED, theEvent);
}

- (void)mouseMoved:(NSEvent *)theEvent
{
    doMouseEvent(MOUSE_MOVED, theEvent);
}

// ---------------------------------

- (void)scrollWheel:(NSEvent *)theEvent
{
    struct OLEvent e;
    memset(&e, 0, sizeof(e));
    e.type = SCROLL_WHEEL;
    e.dx = [theEvent deltaX];
    e.dy = -[theEvent deltaY];

    // actually repect this! "natural" scrolling is "inverted" here.
    // BOOL inverted = [theEvent isDirectionInvertedFromDevice];
    // if (inverted) {
        // e.dy = -e.dy;
    // }
    
    OLG_OnEvent(&e);
}

// ---------------------------------

- (void)rightMouseDragged:(NSEvent *)theEvent
{
    [self mouseDragged: theEvent];
}

- (void)otherMouseDragged:(NSEvent *)theEvent
{
    [self mouseDragged: theEvent];
}

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

- (void)resetCursorRects
{
    [super resetCursorRects];

    [self addCursorRect:[self bounds] cursor:invisibleCursor()];
}

- (void)updateTrackingAreas 
{
    [super updateTrackingAreas];
    [self removeTrackingArea:trackingArea];
    [trackingArea release];
    trackingArea = [[NSTrackingArea alloc] initWithRect:[self frame]
       options: (/*NSTrackingMouseEnteredAndExited |*/
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
        SDL_Event evt;
        while (SDL_PollEvent(&evt))
            Controller_HandleEvent(&evt);
        
        NSRect rectView = [self convertRectToBacking:[self bounds]];
        glViewport (0, 0, rectView.size.width, rectView.size.height);
        // draw the whole game
        OLG_Draw();
    }
}


void OL_WarpCursorPosition(float x, float y)
{
    NSRect window = NSMakeRect(x, [gView bounds].size.height - y,  0, 0);
    NSRect screen = [gView.window convertRectToScreen:window];
    CGWarpMouseCursorPosition(screen.origin);
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
}

// set initial OpenGL state (current context is set)
// called after context is created
- (void) prepareOpenGL
{
    [self setWantsBestResolutionOpenGLSurface:YES];

    OLG_InitGL();
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

- (BOOL)becomeFirstResponder
{
    return  YES;
}

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
    LogMessage(@"Will Enter Fullscreen");
}

- (void)windowDidExitFullScreen:(NSNotification *)notification
{
    self->fullscreen = NO;

    [[NSApplication sharedApplication] setPresentationOptions:NSApplicationPresentationDefault];

    if (!self->closing)
        OLG_SetFullscreenPref(0);
    LogMessage(@"Will Exit Fullscreen");
}

- (void)windowWillClose:(NSNotification *)notification
{
    LogMessage(@"window will close");
    OLG_OnQuit();
}

- (BOOL)windowShouldClose:(id)sender
{
    LogMessage(@"window should close");
    OLG_OnClose();
    return YES;
}

- (void) handleResignKeyNotification: (NSNotification *) notification
{
    struct OLEvent e;
    memset(&e, 0, sizeof(e));
    e.type = LOST_FOCUS;
    OLG_OnEvent(&e);
}

- (void) handleBecomeKeyNotification: (NSNotification *) notification
{
}


static void sigtermHandler()
{
    LogMessage(@"Caught SIGTERM!");
    fflush(NULL);
}

- (void) awakeFromNib
{
    // start animation timer
    timer = [NSTimer timerWithTimeInterval:(1.0f/60.0f) target:self selector:@selector(animationTimer:) userInfo:nil repeats:YES];
    [[NSRunLoop currentRunLoop] addTimer:timer forMode:NSDefaultRunLoopMode];
    [[NSRunLoop currentRunLoop] addTimer:timer forMode:NSEventTrackingRunLoopMode]; // ensure timer fires during resize
    

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

    signal(SIGTERM, sigtermHandler);
    signal(SIGINT, sigtermHandler); 
    
    gView = self;
    self->fullscreen = NO;
    self->closing = 0;

    [[self window] setDelegate:self];

    Controller_Init();
}


@end
