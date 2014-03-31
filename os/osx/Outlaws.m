
// Outlaws.m - Outlaws.h platform core implementation for OSX

#import <Cocoa/Cocoa.h>
#import <OpenGL/OpenGL.h>
#import <OpenGL/CGLContext.h>

#import "BasicOpenGLView.h"
#include "Outlaws.h"

#define ASSERT(X) if (!(X)) OLG_OnAssertFailed(__FILE__, __LINE__, __func__, #X, "")

static void LogMessage(NSString *str)
{
    OL_ReportMessage([str UTF8String]);
}

static BOOL createParentDirectories(NSString *path)
{
    NSError *error = nil;
    NSString *dir = [path stringByDeletingLastPathComponent];
    if (![[NSFileManager defaultManager] createDirectoryAtPath:dir
                                   withIntermediateDirectories:YES attributes:nil error:&error])
    {
        NSLog(@"Error creating directories for %@: %@", path,
              error ? [error localizedFailureReason] : @"unknown");
        return NO;
    }
    return YES;
}

// Warning - can't LogMessage in here! Log might not be open yet
static NSString *getBaseSavePath()
{
    static NSString *path = nil;
    if (!path)
    {
        NSArray *paths = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES);
        if ([paths count] == 0) {
            NSLog(@"Can't find Application Support directory");
            return nil;
        }
        NSString *savePath = [[paths objectAtIndex:0] stringByAppendingPathComponent:
                                                      [NSString stringWithUTF8String: OGL_GetName()]];
        path = [savePath stringByStandardizingPath];
        [path retain];
        NSLog(@"Save path is %@", path);
    }
    return path;
}

static NSString* pathForFileName(const char* fname, const char* flags)
{
    NSString* fullFileName = [NSString stringWithUTF8String:fname];
    if (fname[0] == '/')
        return fullFileName;

#if DEBUG || DEVELOP
    getBaseSavePath();
    
    // read and write output files directly from source code repository
    NSString *base = [[[NSString stringWithCString:__FILE__ encoding:NSUTF8StringEncoding]
                         stringByAppendingPathComponent: @"../../../"]
                         stringByStandardizingPath];
    NSString *path = [base stringByAppendingPathComponent:fullFileName];
#else
    NSString *savepath = [getBaseSavePath() stringByAppendingPathComponent:fullFileName];
    
    if (savepath && (flags[0] == 'w' || 
                     [[NSFileManager defaultManager] fileExistsAtPath:savepath]))
    {
        return savepath;
    }

    NSString *resourcePath = [[NSBundle mainBundle] resourcePath];
    NSString *path = [resourcePath stringByAppendingPathComponent:fullFileName];
#endif
    ASSERT(path);
    return path;
}

const char *OL_PathForFile(const char *fname, const char* mode)
{
    return [pathForFileName(fname, mode) UTF8String];
}

const char *OL_LoadFile(const char *fname)
{
    NSError *error = nil;
    NSString *path = pathForFileName(fname, "r");
    NSString *contents = [NSString stringWithContentsOfFile:path encoding:NSUTF8StringEncoding error:&error];
    
    if (contents == nil)
    {
        //   NSLog(@"Error reading file at %@: %@",
        //                          path, [error localizedFailureReason]);
        return NULL;
    }

    return [contents UTF8String];
}

int OL_SaveFile(const char *fname, const char* data)
{
    NSString *path = pathForFileName(fname, "w");
    NSError *error = nil;

    if (!createParentDirectories(path))
        return 0;

    NSString *nsdata = [NSString stringWithUTF8String:data];
    BOOL success = nsdata && [nsdata writeToFile:path atomically:YES encoding:NSUTF8StringEncoding error:&error];
    
    if (!success)
    {
        LogMessage([NSString stringWithFormat:@"Error writing to file at %@: %@", 
                           path, error ? [error localizedFailureReason] : @"unknown"]);
    }

    return success ? 1 : 0;
}

int OL_RemoveFileOrDirectory(const char* dirname)
{
    NSString *astr = pathForFileName(dirname, "r");
    
    NSRange saverange = [astr rangeOfString:@"data/save"];
    if (saverange.location == NSNotFound)
    {
        LogMessage([NSString stringWithFormat:@"Error, trying to clear invalid path %@", astr]);
        return 0;
    }

    NSError* err = nil;
    BOOL success = [[NSFileManager defaultManager] removeItemAtPath: astr error:&err];
    if (success == NO)
    {
        LogMessage([NSString stringWithFormat:@"Error clearing directory %@: %@", astr, [err localizedFailureReason]]);
    }
    return success ? 1 : 0;
}

int OL_FileDirectoryPathExists(const char* fname)
{
    NSString *astr = pathForFileName(fname, "r");
    BOOL exists = [[NSFileManager defaultManager] fileExistsAtPath:astr];
    return exists ? 1 : 0;
}


struct OutlawTexture OL_LoadTexture(const char* fname)
{
    struct OutlawTexture t;
    memset(&t, 0, sizeof(t));

    NSString* path = pathForFileName(fname, "r");
    CFURLRef texture_url = CFURLCreateWithFileSystemPath(NULL, (__bridge CFStringRef)path, kCFURLPOSIXPathStyle, false);
    
    if (!texture_url)
    {
        LogMessage([NSString stringWithFormat:@"Error getting image url for %s", fname]);
        return t;
    }
    
    CGImageSourceRef image_source = CGImageSourceCreateWithURL(texture_url, NULL);
    if (!image_source || CGImageSourceGetCount(image_source) <= 0)
    {
        LogMessage([NSString stringWithFormat:@"Error loading image %s", fname]);
        CFRelease(texture_url);
        if (image_source)
            CFRelease(image_source);
        return t;
    }
    
    CGImageRef image = CGImageSourceCreateImageAtIndex(image_source, 0, NULL);
    
    unsigned width  = (unsigned)CGImageGetWidth(image);
    unsigned height = (unsigned)CGImageGetHeight(image);
    
    void *data = malloc(width * height * 4);
    
    CGColorSpaceRef color_space = CGColorSpaceCreateDeviceRGB();
    CGContextRef context = CGBitmapContextCreate(data, width, height, 8, width * 4, color_space, kCGImageAlphaPremultipliedFirst);
    
    CGContextDrawImage(context, CGRectMake(0, 0, width, height), image);
    
    GLuint texture_id;
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    //glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE_SGIS);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE_SGIS);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

    glReportError();
    
    CFRelease(context);
    CFRelease(color_space);
    free(data);
    CFRelease(image);
    CFRelease(image_source);
    CFRelease(texture_url);
    
    t.width = width;
    t.height = height;
    t.texnum = texture_id;

    return t;
}

static NSString *g_fontNames[10];

static NSFont* getFont(int fontName, float size)
{
    // NSString *name = ((fontName == OF_Sans) ? @"DejaVu Sans" :
    //                   (fontName == OF_Serif) ? @"DejaVu Serif" :
    //                   @"DejaVu Sans Mono");
    NSFont* font = [NSFont fontWithName:g_fontNames[fontName] size:size];
    return font;
}

void OL_SetFont(int index, const char* file, const char* name)
{
    g_fontNames[index] = [NSString stringWithUTF8String:name];
    [g_fontNames[index] retain];
    getFont(index, 12);         // preload
}


void OL_FontAdvancements(int fontName, float size, struct OLSize* advancements)
{
    NSFont* font = getFont(fontName, size);
    NSGlyph glyphs[128];
    for (uint i=0; i<128; i++) {
        const char cstring[] = {(char)i, '\0'};
        glyphs[i] = [font glyphWithName:[NSString stringWithUTF8String:cstring]];
        if (glyphs[i] == -1)
            LogMessage([NSString stringWithFormat:@"Failed to get glyph for char %d '%s'", i, cstring]);
    }
    NSSize adv[128];
    [font getAdvancements:adv forGlyphs:glyphs count:128];

    for (uint i=0; i<128; i++) {
        advancements[i].x = adv[i].width;
        advancements[i].y = adv[i].height;
    }
}

int OL_StringTexture(OutlawTexture *tex, const char* str, float size, int fontName, float maxw, float maxh)
{
    CGLContextObj cgl_ctx;
    GLuint texName = tex->texnum;
    int status = 1;

    NSString* aString = [NSString stringWithUTF8String:str];
    if (!aString || [aString length] == 0) 
    {
        LogMessage([NSString stringWithFormat:@"Failed to get an NSString for '%s'", str]);
        return 0;
    }

    // !!!!!
    // NSAttributedString always draws at the resolution of the highest attached monitor
    // Regardless of the current monitor.
    // Higher res tex looks bad on lower res monitors, because the pixels don't line up
    // Apparently there is no way to tell it not to do that, but we can lie about the text size...
    const float scale = OL_GetCurrentBackingScaleFactor() / OL_GetBackingScaleFactor();
    size *= scale;
        
    NSMutableDictionary *attribs = [NSMutableDictionary dictionary];
    NSFont* font = getFont(fontName, size);
    [attribs setObject:font forKey: NSFontAttributeName];
    [attribs setObject:[NSColor whiteColor] forKey: NSForegroundColorAttributeName];

    NSAttributedString* string = [[[NSAttributedString alloc] initWithString:aString attributes:attribs] autorelease];

    NSSize previousSize = NSMakeSize(tex->width, tex->height);
    NSSize frameSize = [string size];

    if (frameSize.width == 0.f || frameSize.height == 0.f) 
    {
        LogMessage([NSString stringWithFormat:@"String is zero size: '%s'", str]);
        return 0;
    }
    
    if (maxw > 0.f && maxh > 0.f)
    {
        frameSize.width = fminf(maxw, frameSize.width);
        frameSize.height = fminf(maxh, frameSize.height);
    }
    frameSize.width = ceilf(frameSize.width);
    frameSize.height = ceilf(frameSize.height);

    NSBitmapImageRep * bitmap = nil;
    NSImage * image = [[[NSImage alloc] initWithSize:frameSize] autorelease];

	[image lockFocus];
    {
        const BOOL useAA = size > 10.f;
        [[NSGraphicsContext currentContext] setShouldAntialias:useAA];
        //[[NSColor blackColor] setFill];
        //[NSBezierPath fillRect:NSMakeRect(0, 0, frameSize.width, frameSize.height)];
        @try {
            [string drawAtPoint:NSMakePoint (0, 0)]; // draw at offset position
        } @catch (NSException* exception) {
            LogMessage([NSString stringWithFormat:@"Error drawing string '%s': %@", str, exception.description]);
            LogMessage([NSString stringWithFormat:@"Stack trace: %@", [exception callStackSymbols]]);
            return 0;
        }
        bitmap = [[[NSBitmapImageRep alloc] initWithFocusedViewRect:NSMakeRect (0.0f, 0.0f, frameSize.width, frameSize.height)] autorelease];
    }
	[image unlockFocus];

    NSSize texSize;
	texSize.width = [bitmap pixelsWide];
	texSize.height = [bitmap pixelsHigh];
	
	if ((cgl_ctx = CGLGetCurrentContext ())) { // if we successfully retrieve a current context (required)
		//glPushAttrib(GL_TEXTURE_BIT);
		if (0 == texName)
            glGenTextures (1, &texName);
		glBindTexture (GL_TEXTURE_2D, texName);
        GLenum format = ([bitmap samplesPerPixel] == 4 ? GL_RGBA :
                         [bitmap samplesPerPixel] == 3 ? GL_RGB :
                         [bitmap samplesPerPixel] == 2 ? GL_LUMINANCE_ALPHA : GL_LUMINANCE);
		if (NSEqualSizes(previousSize, texSize)) {
			glTexSubImage2D(GL_TEXTURE_2D,0,0,0,(GLsizei)texSize.width,(GLsizei)texSize.height, format, GL_UNSIGNED_BYTE, [bitmap bitmapData]);
		} else {
			//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);             // it's already antialiased
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA, (GLsizei)texSize.width, (GLsizei)texSize.height, 0, format, GL_UNSIGNED_BYTE, [bitmap bitmapData]);
		}
		//glPopAttrib();
	} else { 
		NSLog (@"StringTexture -genTexture: Failure to get current OpenGL context\n");
        status = 0;
    }

    glReportError();
	
    tex->texnum = texName;
    tex->width = texSize.width;
    tex->height = texSize.height;
    return status;
}


#define THREADS 10
static NSAutoreleasePool* gUpdateThreadPool[THREADS];


void OL_ThreadBeginIteration(int i)
{
    if (i < 0 || i >= THREADS)
        return;
    
    static bool initialized = false;
    if (!initialized)
    {
        // initialized a dummy thread to put Cocoa into multithreading mode
        [[[NSThread new] autorelease] start];
        initialized = true;
    }

    gUpdateThreadPool[i] = [NSAutoreleasePool new];
}


void OL_ThreadEndIteration(int i)
{
    if (i < 0 || i >= THREADS)
        return;

    if (gUpdateThreadPool[i])
    {
        [gUpdateThreadPool[i] drain];
        gUpdateThreadPool[i] = nil;
    }
}

void OL_ReportMessage(const char *str)
{
    static FILE *logfile = NULL;
#if DEBUG
    if (!str)
        __builtin_trap();
#endif
    if (!logfile) {
        NSString *fname = pathForFileName("data/log.txt", "w");
        if (createParentDirectories(fname))
        {
            logfile = fopen([fname UTF8String], "a");
            if (logfile)
                LogMessage([NSString stringWithFormat:@"Opened log at %@", fname]);
            else
                NSLog(@"Error opening log at %@: %s", fname, strerror(errno));
        }
    }
    //NSLog(@"[DBG] %s\n", str);
    fprintf(stderr, "[DBG] %s\n", str);
    if (logfile) {
        fprintf(logfile, "%s\n", str);
    }
}


const char* OL_GetPlatformDateInfo(void)
{
    static NSString *buf = nil;
    if (!buf)
    {
        NSDateFormatter* formatter = [[[NSDateFormatter alloc] init] autorelease];
        formatter.timeZone = [NSTimeZone systemTimeZone];
        [formatter setDateStyle:NSDateFormatterLongStyle];
        [formatter setDateFormat:@"MM/dd/yyyy hh:mma"];

        NSProcessInfo *process = [NSProcessInfo processInfo];
        unsigned long long memoryBytes = [process physicalMemory];
        unsigned long long memoryGB = memoryBytes / (1024 * 1024 * 1024);

        buf = [NSString stringWithFormat:@"OSX %@, %dGB Memory, %d Cores, Time is %@", 
                        [process operatingSystemVersionString],
                        (int)memoryGB,
                        (int)[process processorCount],
                        [formatter stringFromDate:[NSDate date]]];
        [buf retain];
    }
    return [buf UTF8String];
}


