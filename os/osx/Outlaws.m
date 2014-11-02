
// Outlaws.m - Outlaws.h platform core implementation for OSX

#import <Cocoa/Cocoa.h>
#import <OpenGL/OpenGL.h>
#import <OpenGL/CGLContext.h>

#import "BasicOpenGLView.h"
#include "Outlaws.h"

#define ASSERT(X) if (!(X)) OLG_OnAssertFailed(__FILE__, __LINE__, __func__, #X, "")

void LogMessage(NSString *str)
{
    OL_ReportMessage([[NSString stringWithFormat:@"\n[OSX] %@", str] UTF8String]);
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

        NSString *appsupport = [paths objectAtIndex:0];
        NSString *savePath = [appsupport stringByAppendingPathComponent:
                                         [NSString stringWithUTF8String: OLG_GetName()]];
        
        path = [savePath stringByStandardizingPath];
        [path retain];
        NSLog(@"Save path is %@", path);

        NSFileManager *fm = [NSFileManager defaultManager];
        
        if (![fm fileExistsAtPath:path])
        {
            NSString *oldPath = [appsupport stringByAppendingPathComponent:@"Outlaws"];
            if ([fm fileExistsAtPath:oldPath])
            {
                NSError *error = nil;
                BOOL success = [fm moveItemAtPath:oldPath toPath:savePath error:&error];
                NSLog(@"Moved save directory from %@ to %@: %s%@",
                      oldPath, savePath, success ? "OK" : "FAILED",
                      error ? [error localizedFailureReason] : @"");
            }
        }
        
    }
    return path;
}

static NSString* pathForFileName(const char* fname, const char* flags)
{
    NSString* fullFileName = [NSString stringWithUTF8String:fname];
    if (fname[0] == '/')
        return fullFileName;
    
    if (fname[0] == '~')
    {
        return [fullFileName stringByStandardizingPath];
    }

    NSString *path = nil;
    if (OLG_UseDevSavePath())
    {
        getBaseSavePath();
        
        // read and write output files directly from source code repository
        NSString *base = [[[NSString stringWithCString:__FILE__ encoding:NSUTF8StringEncoding]
                              stringByAppendingPathComponent: @"../../../"]
                             stringByStandardizingPath];
        path = [base stringByAppendingPathComponent:fullFileName];
    }
    else
    {
        NSString *savepath = [getBaseSavePath() stringByAppendingPathComponent:fullFileName];
        
        if (savepath && (flags[0] == 'w' ||
                         [[NSFileManager defaultManager] fileExistsAtPath:savepath]))
        {
            return savepath;
        }
        
        NSString *resourcePath = [[NSBundle mainBundle] resourcePath];
        path = [resourcePath stringByAppendingPathComponent:fullFileName];
    }
    ASSERT(path);
    return path;
}

const char *OL_PathForFile(const char *fname, const char* mode)
{
    return [pathForFileName(fname, mode) UTF8String];
}


const char** OL_ListDirectory(const char* path)
{
    static const int kMaxElements = 500;
    static const char* array[kMaxElements];

    NSString *nspath = pathForFileName(path, "r");
    NSError *error = nil;
    NSArray *dirFiles = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:nspath error:&error];
    if (!dirFiles || error)
    {
        // do this a lot to test for existence
        // LogMessage([NSString stringWithFormat:@"Error listing directory %@: %@",
                             // nspath, error ? [error localizedFailureReason] : @"unknown"]);
        return NULL;
    }

    int i = 0;
    for (NSString* file in dirFiles)
    {
        if (i >= kMaxElements-1)
            break;
        
        array[i] = [file UTF8String];
        i++;
    }

    array[i] = NULL;

    return array;
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

int OL_SaveFile(const char *fname, const char* data, int size)
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

struct OutlawImage OL_LoadImage(const char* fname)
{
    struct OutlawImage t;
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
    
    t.width  = (unsigned)CGImageGetWidth(image);
    t.height = (unsigned)CGImageGetHeight(image);
    
    t.data = calloc(t.width * t.height, 4);
    if (t.data)
    {
        CGColorSpaceRef color_space = CGColorSpaceCreateDeviceRGB();
        CGContextRef context = CGBitmapContextCreate(t.data, t.width, t.height, 8, t.width * 4, color_space, kCGImageAlphaPremultipliedFirst);
        
        CGContextDrawImage(context, CGRectMake(0, 0, t.width, t.height), image);
        
        CFRelease(context);
        CFRelease(color_space);
    }
    CFRelease(image);
    CFRelease(image_source);
    CFRelease(texture_url);
    
    return t;
}


struct OutlawTexture OL_LoadTexture(const char* fname)
{
    struct OutlawTexture t;
    memset(&t, 0, sizeof(t));
    
    struct OutlawImage image = OL_LoadImage(fname);
    if (!image.data)
        return t;
    
    GLuint texture_id;
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    //glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width, image.height, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8, image.data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE_SGIS);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE_SGIS);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

    glReportError();
    
    free(image.data);

    t.width = image.width;
    t.height = image.height;
    t.texnum = texture_id;

    return t;
}

int OL_SaveTexture(const OutlawTexture *tex, const char* fname)
{
    if (!tex || !tex->texnum || tex->width <= 0 || tex->height <= 0)
        return 0;

    const size_t size = tex->width * tex->height * 4;
    uint *pix = malloc(size);
    
    glBindTexture(GL_TEXTURE_2D, tex->texnum);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pix);
    glReportError();

    // invert image
    for (int y=0; y<tex->height/2; y++)
    {
        for (int x=0; x<tex->width; x++)
        {
            const int top = y * tex->width + x;
            const int bot = (tex->height - y - 1) * tex->width + x;
            const uint temp = pix[top];
            pix[top] = pix[bot];
            pix[bot] = temp;
        }
    }

    CGImageRef cimg = nil;
    {
        CGColorSpaceRef color_space = CGColorSpaceCreateDeviceRGB();
        CGDataProviderRef provider = CGDataProviderCreateWithData(NULL, pix, size, NULL);
        cimg = CGImageCreate(tex->width, tex->height, 8, 32, tex->width * 4,
                             color_space, kCGImageAlphaLast, provider,
                             NULL, FALSE, kCGRenderingIntentDefault);

        CGDataProviderRelease(provider);
        CFRelease(color_space);
    }

    NSString* path = pathForFileName(fname, "w");
    BOOL success = false;
    
    if (createParentDirectories(path))
    {
        CFURLRef url = CFURLCreateWithFileSystemPath(NULL, (__bridge CFStringRef)path, kCFURLPOSIXPathStyle, false);
        CGImageDestinationRef dest = CGImageDestinationCreateWithURL(url, kUTTypePNG, 1, NULL);
        CGImageDestinationAddImage(dest, cimg, NULL);
        
        success = CGImageDestinationFinalize(dest);
        CFRelease(dest);
        CFRelease(url);
    }
    
    CGImageRelease(cimg);
    free(pix);
    
    if (!success) {
        LogMessage([NSString stringWithFormat:@"Failed to write texture %d-%dx%d to '%s'",
                             tex->texnum, tex->width, tex->height, fname]);
    }
    return success;
}

static NSString *g_fontNames[10];

static NSFont* getFont(int fontName, float size)
{
    // NSString *name = ((fontName == OF_Sans) ? @"DejaVu Sans" :
    //                   (fontName == OF_Serif) ? @"DejaVu Serif" :
    //                   @"DejaVu Sans Mono");
    NSFont* font = [NSFont fontWithName:g_fontNames[fontName] size:size];
    if (!font) {
        LogMessage([NSString stringWithFormat:@"Failed to load font %d '%@' size %g", fontName, g_fontNames[fontName], size]);
    }
    return font;
}

void OL_SetFont(int index, const char* file, const char* name)
{
    g_fontNames[index] = [[NSString alloc] initWithUTF8String:name];
    getFont(index, 12);         // preload
}\


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
    // NSRect adv[128];
    // [font getBoundingRects:adv forGlyphs:glyphs count:128];
    for (uint i=0; i<128; i++) {
        advancements[i].x = adv[i].width + 0.7; // WTF why is this off
        advancements[i].y = adv[i].height;
        // advancements[i].x = adv[i].size.width;
        // advancements[i].y = adv[i].size.height;
    }
}

float OL_FontHeight(int fontName, float size)
{
    static NSLayoutManager *lm = nil;
    if (!lm)
    {
        lm = [[NSLayoutManager alloc] init];
    }
    NSFont* font = getFont(fontName, size);
    return [lm defaultLineHeightForFont: font];
}

int OL_StringTexture(OutlawTexture *tex, const char* str, float size, int fontName, float maxw, float maxh)
{
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
        
    NSFont* font = getFont(fontName, size);
    if (!font)
        return 0;

    NSMutableDictionary *attribs = [NSMutableDictionary dictionary];
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
        //[[NSCol=or blackColor] setFill];
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

    if (!CGLGetCurrentContext())
    {
		NSLog (@"StringTexture -genTexture: Failure to get current OpenGL context\n");
        return 0;        
    }
    
    if (tex->texnum == 0)
        glGenTextures (1, &tex->texnum);
    glBindTexture (GL_TEXTURE_2D, tex->texnum);
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

    glReportError();
	
    tex->width = texSize.width;
    tex->height = texSize.height;
    return 1;
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

static FILE *g_logfile = NULL;

void OL_ReportMessage(const char *str)
{
#if DEBUG
    if (!str)
        __builtin_trap();
#endif
    if (!g_logfile) {
        NSString *fname = pathForFileName(OLG_GetLogFileName(), "w");
        if (createParentDirectories(fname))
        {
            const char* logpath = [fname UTF8String];
            g_logfile = fopen(logpath, "w");
            NSString *tildeFname = [fname stringByAbbreviatingWithTildeInPath];
            if (g_logfile) {
                LogMessage([NSString stringWithFormat:@"Opened log at %@", tildeFname]);
                const char* latestpath = [pathForFileName("data/log_latest.txt", "w") UTF8String];
                int status = unlink(latestpath);
                if (status && errno != ENOENT)
                    LogMessage([NSString stringWithFormat:@"Error unlink('%s'): %s", latestpath, strerror(errno)]);
                if (symlink(logpath, latestpath))
                    LogMessage([NSString stringWithFormat:@"Error symlink('%s', '%s'): %s",
                                         logpath, latestpath, strerror(errno)]);
            } else {
                NSLog(@"Error opening log at %@: %s", tildeFname, strerror(errno));
            }
        }
    }
    //NSLog(@"[DBG] %s\n", str);
    fprintf(stderr, "%s", str);
    if (g_logfile) {
        fprintf(g_logfile, "%s", str);
    }
}


int OL_GetCpuCount(void)
{
    NSProcessInfo *process = [NSProcessInfo processInfo];
    return (int) [process processorCount];
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

const char* OL_ReadClipboard()
{
    static NSArray *classes = nil;
    static NSDictionary *options = nil;
    if (!classes) {
        classes = [[NSArray alloc] initWithObjects:[NSString class], nil];
        options = [[NSDictionary alloc] init];
    }

    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    NSArray *copiedItems = [pasteboard readObjectsForClasses:classes options:options];
    
    if (copiedItems == nil || [copiedItems count] == 0)
        return nil;

    NSString *data = [copiedItems firstObject];
    return [data UTF8String];
}

const char* OL_GetUserName(void)
{
    return [NSUserName() UTF8String];
}

int OL_CopyFile(const char* source, const char* dest)
{
    NSString *src = pathForFileName(source, "r");
    NSString *dst = pathForFileName(dest, "w");

    if (!createParentDirectories(dst))
        return 0;

    NSError *error = nil;
    if ([[NSFileManager defaultManager] copyItemAtPath:src toPath:dst error:&error])
        return 1;

    LogMessage([NSString stringWithFormat:@"Error copying file from '%@' to '%@': %@", 
                         src, dst, error ? [error localizedFailureReason] : @"unknown"]);
    return 0;
}

