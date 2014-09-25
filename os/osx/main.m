//
//  main.m
//  Outlaws
//
//  Created by Arthur Danskin on 10/21/12.
//  Copyright (c) 2012 Tiny Chocolate Games. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#include "Outlaws.h"

void LogMessage(NSString *str);

int main(int argc, char *argv[])
{
    const int mode = OLG_Init(argc, (const char**)argv);
    if (mode == 0)
    {
        static NSOpenGLPixelFormatAttribute glAttributes[] = {
            NSOpenGLPFADepthSize, (NSOpenGLPixelFormatAttribute)16, // 16 bit depth buffer
        };
        NSOpenGLPixelFormat *pixelFormat = [[NSOpenGLPixelFormat alloc]
                                               initWithAttributes:glAttributes];
        NSOpenGLContext *ctx = [[NSOpenGLContext alloc]
                                initWithFormat:pixelFormat shareContext:nil];
        [ctx makeCurrentContext];
        OLG_Draw();
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
        return 0;
    }
}
