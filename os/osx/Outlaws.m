
// Outlaws.m - Outlaws.h platform core implementation for OSX

#import <Cocoa/Cocoa.h>
#import <OpenGL/OpenGL.h>
#import <OpenGL/CGLContext.h>
// @import CoreGraphics

#include <cpuid.h>
#include <pthread.h>

#import "BasicOpenGLView.h"
#include "Outlaws.h"

#include "posix.h"

typedef unsigned int uint;
#include "Colors.h"

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

static NSString *getDevSavePath()
{
    static NSString *base = nil;
    if (base == nil)
    {
        base = [[[[NSString stringWithUTF8String:__FILE__]
                              stringByAppendingPathComponent: @"../../../"]
                    stringByStandardizingPath] retain];
    }
    return base;
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

    if (OLG_UseDevSavePath())
    {
        getBaseSavePath();      // detect bugs
        
        // read and write output files directly from source code repository
        return [getDevSavePath() stringByAppendingPathComponent:fullFileName];
    }
    
    if (flags[0] != 'p')
    {
        NSString *savepath = [getBaseSavePath() stringByAppendingPathComponent:fullFileName];
        
        if (savepath && (flags[0] == 'w' || flags[0] == 'l' ||
                         [[NSFileManager defaultManager] fileExistsAtPath:savepath]))
        {
            return savepath;
        }
    }
    // no resources build has nothing in the bundle!
#if NORES
    return [getDevSavePath() stringByAppendingPathComponent:fullFileName];
#else
    return [[[NSBundle mainBundle] resourcePath] stringByAppendingPathComponent:fullFileName];
#endif
}

const char *OL_PathForFile(const char *fname, const char* mode)
{
    return [pathForFileName(fname, mode) UTF8String];
}

int OL_DirectoryExists(const char* path)
{
    NSString *nspath = pathForFileName(path, "r");
    BOOL isdir = NO;
    BOOL exists = [[NSFileManager defaultManager] fileExistsAtPath:nspath isDirectory:&isdir];
    return exists && isdir;
}

// get case insensitive path
static NSString *caseFilePath(NSString* path)
{
    NSFileManager *mgr = [NSFileManager defaultManager];
    const char *cFilePath = [mgr fileSystemRepresentationWithPath:path];
    if (!cFilePath || *cFilePath == '\0')
        return path;

    int len = PATH_MAX + 1;
    char cRealPath[len];
    memset(cRealPath, 0, len);
    char *result = realpath(cFilePath, cRealPath);
    return result ? [mgr stringWithFileSystemRepresentation:result length:strlen(result)] : path;
}

static NSArray *listDirectory(const char* path, const char* mode)
{
    NSString *nspath = pathForFileName(path, mode);
    nspath = caseFilePath(nspath);
    NSError *error = nil;
    NSArray *dirFiles = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:nspath error:&error];
    if (!dirFiles || error)
    {
        return nil;
    }
    return dirFiles;
}

const char** OL_ListDirectory(const char* path)
{
    NSArray *package = listDirectory(path, "p");
    NSArray *local = listDirectory(path, "w");

    NSMutableSet *files = package ? [NSMutableSet setWithArray:package] : nil;
    if (package && local) {
        [files unionSet:[NSSet setWithArray:local]];
    } else if (local) {
        files = [NSMutableSet setWithArray:local];
    }
    
    if (!files)
        return NULL;
    
    static const char** array = NULL;
    static int arraysize = 0;
    
    if (!array || arraysize <= [files count])
    {
        arraysize = [files count] + 1;
        if (array)
            array = realloc(array, sizeof(const char*) * arraysize);
        else
            array = calloc(sizeof(const char*), arraysize);
    }
    
    int i = 0;
    for (NSString* file in files)
    {
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

    // LogMessage([NSString stringWithFormat:@"load %@", path]);
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
    struct OutlawImage img;
    memset(&img, 0, sizeof(img));

    NSString* path = pathForFileName(fname, "r");
    CFURLRef texture_url = CFURLCreateWithFileSystemPath(NULL, (__bridge CFStringRef)path, kCFURLPOSIXPathStyle, false);
    
    if (!texture_url)
    {
        LogMessage([NSString stringWithFormat:@"Error getting image url for %s", fname]);
        return img;
    }
    
    CGImageSourceRef image_source = CGImageSourceCreateWithURL(texture_url, NULL);
    if (!image_source || CGImageSourceGetCount(image_source) <= 0)
    {
        LogMessage([NSString stringWithFormat:@"Error loading image %s", fname]);
        CFRelease(texture_url);
        if (image_source)
            CFRelease(image_source);
        return img;
    }
    
    CGImageRef image = CGImageSourceCreateImageAtIndex(image_source, 0, NULL);
    
    img.width  = (unsigned)CGImageGetWidth(image);
    img.height = (unsigned)CGImageGetHeight(image);

    img.format = GL_BGRA;
    img.type = GL_UNSIGNED_INT_8_8_8_8;
    img.data = calloc(img.width * img.height, 4);
    img.handle = img.data;
    
    if (img.data)
    {
        CGColorSpaceRef color_space = CGColorSpaceCreateDeviceRGB();
        CGContextRef context = CGBitmapContextCreate(img.data, img.width, img.height, 8, img.width * 4, color_space,
                                                     kCGImageAlphaPremultipliedFirst);
        
        CGContextDrawImage(context, CGRectMake(0, 0, img.width, img.height), image);
        
        CFRelease(context);
        CFRelease(color_space);
    }
    CFRelease(image);
    CFRelease(image_source);
    CFRelease(texture_url);

    // invert_image((uint*)img.data, img.width, img.height);
    
    return img;
}

void OL_FreeImage(OutlawImage *img)
{
    if (img->handle)
        free(img->handle);
}

int OL_SaveImage(const OutlawImage *img, const char* fname)
{
    if (!img || !img->data || img->width <= 0 || img->height <= 0)
        return 0;

    CGImageRef cimg = nil;
    {
        CGColorSpaceRef color_space = CGColorSpaceCreateDeviceRGB();
        const int size = 4 * img->width * img->height;
        CGDataProviderRef provider = CGDataProviderCreateWithData(NULL, img->data, size, NULL);
        cimg = CGImageCreate(img->width, img->height, 8, 32, img->width * 4,
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
    
    if (!success) {
        LogMessage([NSString stringWithFormat:@"Failed to write image %dx%d to '%s'",
                             img->width, img->height, fname]);
    }
    return success;
}

static NSString *g_fontPaths[OL_MAX_FONTS];
static CGFontRef g_fonts[OL_MAX_FONTS];
static NSLock   *g_fontMutex;

static NSFont* getFont(int index, float size)
{
    [g_fontMutex lock];
    if (!g_fonts[index])
    {
        // see http://devmacosx.blogspot.com/2012/03/nsfont-from-file.html
        CGDataProviderRef dataProvider = CGDataProviderCreateWithFilename([g_fontPaths[index] UTF8String]);
        if (dataProvider)
        {
            g_fonts[index] = CGFontCreateWithDataProvider(dataProvider);
            CGDataProviderRelease(dataProvider);
        }
    }

    CTFontRef font = nil;
    if (g_fonts[index])
        font = CTFontCreateWithGraphicsFont(g_fonts[index], size, NULL, NULL);
    if (!font)
        LogMessage([NSString stringWithFormat:@"Failed to load font %d from '%@' size %g", index, g_fontPaths[index], size]);
    [g_fontMutex unlock];
    NSFont *ns = [(NSFont*)font autorelease];
    return ns;
}

void OL_SetFont(int index, const char* file)
{
    [g_fontMutex lock];
    g_fontPaths[index] = [pathForFileName(file, "r") retain];
    if (g_fonts[index]) {
        CFRelease(g_fonts[index]);
        g_fonts[index] = nil;
    }
    [g_fontMutex unlock];
    LogMessage([NSString stringWithFormat:@"Found font %d at %s: %s",
                         index, file, getFont(index, 12) ? "OK" : "FAILED"]);
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
    return 0.9f * [lm defaultLineHeightForFont: font];
}

static NSAttributedString *createColorString(const char* str, NSFont *font, NSColor *color)
{
    NSString *astring = [NSString stringWithUTF8String:str];
    if (!astring)
    {
        LogMessage([NSString stringWithFormat:@"Failed to get an NSString for '%s'", str]);
        return nil;
    }
    
    NSMutableDictionary *attribs = [NSMutableDictionary dictionary];
    [attribs setObject:font forKey: NSFontAttributeName];
    [attribs setObject:color forKey: NSForegroundColorAttributeName];
        
    return  [[[NSAttributedString alloc] initWithString:astring attributes:attribs] autorelease];
}

static int appendQuake3String(NSMutableAttributedString *mstring, NSFont *font, 
                               const char* str, int len, int cc)
{
    if (len <= 0)
        return 0;
    const uint color = OLG_GetQuake3Color(cc);
    NSColor *nsc = [NSColor colorWithDeviceRed:(color>>16) / 255.0
                                         green:((color>>8)&0xff)/255.0
                                          blue:(color&0xff)/255.0
                                         alpha: 1.0];
    NSString *astring = [[[NSString alloc] initWithBytes:str length:len encoding:NSUTF8StringEncoding] autorelease];
    if (!astring) 
    {
        LogMessage([NSString stringWithFormat:@"Failed to get an NSString for '%s'", str]);
        return 0;
    }
    
    NSMutableDictionary *attribs = [NSMutableDictionary dictionary];
    [attribs setObject:font forKey: NSFontAttributeName];
    [attribs setObject:nsc forKey: NSForegroundColorAttributeName];
    
    NSAttributedString* string = [[[NSAttributedString alloc] initWithString:astring attributes:attribs] autorelease];
    [mstring appendAttributedString:string];
    return 1;
}

int OL_StringImage(struct OutlawImage *img, const char* str, float size, int fontName, float maxw, float maxh)
{
    // !!!!!
    // NSAttributedString always draws at the resolution of the highest attached monitor
    // Regardless of the current monitor.
    // Higher res tex looks bad on lower res monitors, because the pixels don't line up
    // Apparently there is no way to tell it not to do that, but we can lie about the text size...
    size *= OL_GetCurrentBackingScaleFactor() / getBackingScaleFactor();

    NSFont* font = getFont(fontName, size);
    if (!font)
        return 0;
    
    NSAttributedString* string = nil;
    NSMutableAttributedString *mstring = nil;

    int len = 0;
    int lastV = 7;
    const char* lastPtr = str;
    
    for (const char *ptr=str; *ptr; ptr++, len++)
    {
        if (*ptr == '^' && '0' <= ptr[1] && ptr[1] <= '9')
        {
            if (!mstring)
                mstring = [[NSMutableAttributedString alloc] init];
            appendQuake3String(mstring, font, lastPtr, len, lastV);

            lastV = ptr[1] - '0';
            lastPtr = ptr + 2;
            len = -2;
        }
    }
    
    if (mstring) {
        appendQuake3String(mstring, font, lastPtr, len, lastV);
        string = mstring;
    } else {
        string = createColorString(str, font, [NSColor whiteColor]);
    }
        
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
        CGContextRef ctx = [[NSGraphicsContext currentContext] graphicsPort];
        CGContextSetShouldAntialias(ctx, YES);
        // CContextSetShouldSmoothFonts(ctx, YES);
        CGContextSetShouldSubpixelPositionFonts(ctx, YES);
        CGContextSetShouldSubpixelQuantizeFonts(ctx, YES);
        // const BOOL useAA = size >= 7.f;
        // [[NSColor blackColor] setFill];
        // [NSBezierPath fillRect:NSMakeRect(0, 0, frameSize.width, frameSize.height)];
        @try {
            [string drawAtPoint:NSMakePoint(0.5, 0.5)]; // draw at offset position
            // [string drawAtPoint:NSMakePoint(0.0, 0.0)]; // draw at offset position
        } @catch (NSException* exception) {
            LogMessage([NSString stringWithFormat:@"Error drawing string '%s': %@", str, exception.description]);
            LogMessage([NSString stringWithFormat:@"Stack trace: %@", [exception callStackSymbols]]);
            return 0;
        }
        bitmap = [[[NSBitmapImageRep alloc] initWithFocusedViewRect:NSMakeRect (0.0f, 0.0f, frameSize.width, frameSize.height)] autorelease];
    }
	[image unlockFocus];

    img->width = [bitmap pixelsWide];
    img->height = [bitmap pixelsHigh];
    img->internal_format = mstring ? GL_RGBA : GL_LUMINANCE_ALPHA;
    img->format = ([bitmap samplesPerPixel] == 4 ? GL_RGBA :
                   [bitmap samplesPerPixel] == 3 ? GL_RGB :
                   [bitmap samplesPerPixel] == 2 ? GL_LUMINANCE_ALPHA : GL_LUMINANCE);
    img->type = GL_UNSIGNED_BYTE;
    img->data = (char*)[bitmap bitmapData];
    img->handle = NULL;         // autoreleased

    return 1;
}

static NSMutableDictionary* gUpdateThreadPool;
static NSLock *gPoolMutex;

static NSNumber* current_thread_id(void)
{
    uint64 tid = 0;
    pthread_threadid_np(pthread_self(), &tid);
    return [NSNumber numberWithUnsignedLongLong:tid];
}

void OL_ThreadBeginIteration(void)
{
    if (!gUpdateThreadPool)
    {
        // initialized a dummy thread to put Cocoa into multithreading mode
        [[[NSThread new] autorelease] start];
        gUpdateThreadPool = (NSMutableDictionary*)CFDictionaryCreateMutable(nil, 0, &kCFCopyStringDictionaryKeyCallBacks, NULL);
        gPoolMutex = [[NSLock alloc] init];
    }

    [gPoolMutex lock];
    [gUpdateThreadPool setObject:[NSAutoreleasePool new] forKey:current_thread_id()];
    [gPoolMutex unlock];
}


void OL_ThreadEndIteration(void)
{
    [gPoolMutex lock];
    NSAutoreleasePool *pool = [gUpdateThreadPool objectForKey:current_thread_id()];
    if (pool)
    {
        [pool drain];
    }
    [gPoolMutex unlock];
}

FILE *g_logfile = nil;
NSString *g_logpath = nil;

void OL_ReportMessage(const char *str)
{
#if DEBUG
    if (!str)
        __builtin_trap();
#endif
    if (!g_logfile) {
        NSString *fname = pathForFileName(OLG_GetLogFileName(), "w");
        g_logpath = [fname retain];
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

// in Str.cpp
const char* str_cpuid_(void);

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
        const unsigned long long memoryBytes = [process physicalMemory];
        const double memoryGB = memoryBytes / (1024.0 * 1024.0 * 1024.0);

        buf = [NSString stringWithFormat:@"OSX %@ %@, %s with %d cores %.1fGB, %@",
                        [process operatingSystemVersionString],
                        [[NSLocale currentLocale] localeIdentifier],
                        str_cpuid_(), (int)[process processorCount], memoryGB,
                        [formatter stringFromDate:[NSDate date]]];
        [buf retain];
    }
    return [buf UTF8String];
}

const char** OL_GetOSLanguages(void)
{
    static const int kLangMax = 10;
    static const char *val[kLangMax];
    if (!val)
    {
        NSArray *languages = [NSLocale preferredLanguages];
        int i=0;
        for (NSString* lang in languages)
        {
            if (i >= kLangMax-1)
                break;
            val[i] = [[lang retain] UTF8String];
            i++;
        }
        val[i] = NULL;
    }
    return (const char**)val;
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

void OL_WriteClipboard(const char* txt)
{
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    NSString *str = [NSString stringWithUTF8String:txt];
    [pasteboard clearContents];
    [pasteboard writeObjects:[NSArray arrayWithObject:str]];
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

int OL_OpenWebBrowser(const char* url)
{
    [[NSWorkspace sharedWorkspace] openURL: [NSURL URLWithString:[NSString stringWithUTF8String: url]]];
    return 1;
}

int OL_HasTearControl(void)
{
    return 0;
}

void OL_ScheduleUploadLog(const char* reason)
{
    // nothing to do
}

void dump_loaded_shared_objects()
{
    // FIXME implement me
}

void posix_oncrash(const char* msg)
{
    if (!g_logpath || !g_logfile)
        return;
    LogMessage([NSString stringWithFormat:@"%s", msg]);
    fclose(g_logfile);
    g_logfile = nil;
    NSString *contents = [NSString stringWithContentsOfFile:g_logpath encoding:NSUTF8StringEncoding error:nil];
    if (!contents)
        return;
    const char* buf = [contents UTF8String];
    OLG_UploadLog(buf, strlen(buf));
    LogMessage(@"\nGoodbye!\n");
}
