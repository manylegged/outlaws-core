//
//  AppDelegate.m
//  Outlaws
//
//  Created by Arthur Danskin on 10/21/12.
//  Copyright (c) 2012 Tiny Chocolate Games. All rights reserved.
//

#import "AppDelegate.h"
#include "Outlaws.h"

@implementation AppDelegate

- (void)dealloc
{
    [super dealloc];
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    // Insert code here to initialize your application
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender
{
    OLG_OnQuit();
    return NSTerminateNow;
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)theApplication
{
    return YES;
}

@end
