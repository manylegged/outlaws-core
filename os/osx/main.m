//
//  main.m
//  Outlaws
//
//  Created by Arthur Danskin on 10/21/12.
//  Copyright (c) 2012 Tiny Chocolate Games. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#include "Outlaws.h"

int main(int argc, char *argv[])
{
    @try 
    {
        return NSApplicationMain(argc, (const char **)argv);
    } 
    @catch (NSException* exception) 
    {
        NSString *msg = [NSString stringWithFormat: @"Uncaught exception: %@\nStack trace: %@",
                                  exception.description, [exception callStackSymbols]];
        OL_ReportMessage([msg cStringUsingEncoding:NSUTF8StringEncoding]);
        return 0;
    }
}
