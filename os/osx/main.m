//
//  main.m
//  Outlaws
//
//  Created by Arthur Danskin on 10/21/12.
//  Copyright (c) 2012 Tiny Chocolate Games. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#include "Outlaws.h"
#include "posix.h"

void LogMessage(NSString *str);

int main(int argc, char *argv[])
{
    const int mode = OLG_Init(argc, (const char**)argv);

    if (OLG_EnableCrashHandler())
        posix_set_signal_handler();

    if (mode == 0)
    {
        NSOpenGLPixelFormatAttribute glAttributes[] = {
            NSOpenGLPFADepthSize, (NSOpenGLPixelFormatAttribute)16, // 16 bit depth buffer
            (NSOpenGLPixelFormatAttribute)nil
        };
        NSOpenGLPixelFormat *pixelFormat = [[NSOpenGLPixelFormat alloc]
                                               initWithAttributes:glAttributes];
        NSOpenGLContext *ctx = [[NSOpenGLContext alloc]
                                initWithFormat:pixelFormat shareContext:nil];
        [ctx makeCurrentContext];

        OLG_InitGL(NULL);

        OLG_Draw();
        
        LogMessage(@"Goodbye!\n");
        return 0;
    }
    
    @try 
    {
        return NSApplicationMain(argc, (const char **)argv);
    } 
    @catch (NSException* exception) 
    {
        LogMessage([NSString stringWithFormat: @"Uncaught exception: %@\nStack trace: %@",
                             exception.description, [exception callStackSymbols]]);
        posix_oncrash([[NSString stringWithFormat: @"Uncaught exception: %@",
                             exception.description] UTF8String]);
        return 0;
    }
}
