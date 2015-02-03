//
//  PlatformIncludes.h
//  Outlaws
//
//  Created by Arthur Danskin on 10/21/12.
//  Copyright (c) 2012 Tiny Chocolate Games. All rights reserved.
//

#ifndef Outlaws_PlatformIncludes_h
#define Outlaws_PlatformIncludes_h

#if defined(DEBUG) || defined(DEVELOP)
#ifdef _LIBCPP_INLINE_VISIBILITY
#undef _LIBCPP_INLINE_VISIBILITY
#endif
#define _LIBCPP_INLINE_VISIBILITY
#endif

#import <OpenGL/gl.h>
#import <OpenGL/glext.h>
#import <OpenGL/glu.h>

#define HAS_SOUND 1

#endif
