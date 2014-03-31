// -*- mode: Objc -*-

#import <OpenGL/gl.h>
#import <OpenGL/glext.h>
#import <OpenGL/glu.h>

GLenum glReportError (void);

@interface BasicOpenGLView : NSOpenGLView<NSWindowDelegate>
{
	NSTimer* timer;
 
	CFAbsoluteTime time;

    NSTrackingArea *trackingArea;
    BOOL            focused;

@public
    int  closing;
    BOOL fullscreen;
}

+ (NSOpenGLPixelFormat*) basicPixelFormat;

- (void)animationTimer:(NSTimer *)timer;

- (void)keyDown:(NSEvent *)theEvent;

- (void) mouseDown:(NSEvent *)theEvent;
- (void) rightMouseDown:(NSEvent *)theEvent;
- (void) otherMouseDown:(NSEvent *)theEvent;
- (void) mouseUp:(NSEvent *)theEvent;
- (void) rightMouseUp:(NSEvent *)theEvent;
- (void) otherMouseUp:(NSEvent *)theEvent;
- (void) mouseDragged:(NSEvent *)theEvent;
- (void) scrollWheel:(NSEvent *)theEvent;
- (void) rightMouseDragged:(NSEvent *)theEvent;
- (void) otherMouseDragged:(NSEvent *)theEvent;
- (void)mouseMoved:(NSEvent *)theEvent;

- (void) drawRect:(NSRect)rect;

- (void) prepareOpenGL;
- (void) update;		// moved or resized

- (BOOL) acceptsFirstResponder;
- (BOOL) becomeFirstResponder;
- (BOOL) resignFirstResponder;

- (id) initWithFrame: (NSRect) frameRect;
- (void) awakeFromNib;

@end
