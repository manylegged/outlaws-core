//
// Event.h - Input tracking utils
// 

// Copyright (c) 2014 Arthur Danskin
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#ifndef CORE_EVENT_H
#define CORE_EVENT_H

inline string keyToString(int key);

struct Event {
    // !!! KEEP IN SYNC WITH Outlaws.h VERSION !!!
    enum Type { KEY_DOWN=0, KEY_UP, MOUSE_DOWN, MOUSE_UP, MOUSE_DRAGGED, 
                MOUSE_MOVED, SCROLL_WHEEL, LOST_FOCUS, GAINED_FOCUS,
                TOUCH_BEGIN, TOUCH_MOVED, TOUCH_STATIONARY, TOUCH_ENDED, TOUCH_CANCELLED,
                INVALID };
    Type   type = INVALID;
    int    key = 0;
    int    rawkey = 0;
    float2 pos;
    float2 vel;

    string toString() const
    {
        const string skey = keyToString(key);
        switch(type)
        {
        case KEY_DOWN:      return str_format("KEY_DOWN %s",     skey.c_str());
        case KEY_UP:        return str_format("KEY_UP %s",       skey.c_str());
        case MOUSE_DOWN:    return str_format("MOUSE_DOWN %d (%.f, %.f)", (int)key, pos.x, pos.y);
        case MOUSE_UP:      return str_format("MOUSE_UP %d (%.f, %.f)", (int)key, pos.x, pos.y);
        case MOUSE_DRAGGED: return str_format("MOUSE_DRAGGED %d (%.f, %.f)",(int)key, pos.x, pos.y);
        case MOUSE_MOVED:   return str_format("MOUSE_MOVED %d, (%.f, %.f)",  (int)key, pos.x, pos.y);
        case SCROLL_WHEEL:  return str_format("SCROLL_WHEEL %g", vel.y);
        case LOST_FOCUS:    return str_format("LOST_FOCUS");
        default:            return "unknown";
        }
    }

    bool isMouse() const
    {
        switch (type) {
        case MOUSE_DOWN:
        case MOUSE_UP:
        case MOUSE_DRAGGED:
        case MOUSE_MOVED:
            return true;
        default:
            return false;
        }
    }

    bool isKey() const
    {
        switch (type) {
        case KEY_DOWN:
        case KEY_UP:
            return true;
        default:
            return false;
        }
    }

    bool isDown() const
    {
        return type == KEY_DOWN || type == MOUSE_DOWN;
    }

    bool isUp() const
    {
        return type == KEY_UP || type == MOUSE_UP;
    }
};

enum : int {
    NSEnterCharacter                = 0x0003,
    NSBackspaceCharacter            = 0x0008,
    NSTabCharacter                  = 0x0009,
    NSNewlineCharacter              = 0x000a,
    NSFormFeedCharacter             = 0x000c,
    NSCarriageReturnCharacter       = 0x000d,
    EscapeCharacter                 = '\033',
    NSBackTabCharacter              = 0x0019,
    NSDeleteCharacter               = 0x007f,
    NSLineSeparatorCharacter        = 0x2028,
    NSParagraphSeparatorCharacter   = 0x2029,

    NSUpArrowFunctionKey = 0xF700,
    NSDownArrowFunctionKey = 0xF701,
    NSLeftArrowFunctionKey = 0xF702,
    NSRightArrowFunctionKey = 0xF703,
    NSF1FunctionKey = 0xF704,
    NSF2FunctionKey = 0xF705,
    NSF3FunctionKey = 0xF706,
    NSF4FunctionKey = 0xF707,
    NSF5FunctionKey = 0xF708,
    NSF6FunctionKey = 0xF709,
    NSF7FunctionKey = 0xF70A,
    NSF8FunctionKey = 0xF70B,
    NSF9FunctionKey = 0xF70C,
    NSF10FunctionKey = 0xF70D,
    NSF11FunctionKey = 0xF70E,
    NSF12FunctionKey = 0xF70F,
    NSF13FunctionKey = 0xF710,
    NSF14FunctionKey = 0xF711,
    NSF15FunctionKey = 0xF712,
    NSF16FunctionKey = 0xF713,
    NSF17FunctionKey = 0xF714,
    NSF18FunctionKey = 0xF715,
    NSF19FunctionKey = 0xF716,
    NSF20FunctionKey = 0xF717,
    NSF21FunctionKey = 0xF718,
    NSF22FunctionKey = 0xF719,
    NSF23FunctionKey = 0xF71A,
    NSF24FunctionKey = 0xF71B,
    NSF25FunctionKey = 0xF71C,
    NSF26FunctionKey = 0xF71D,
    NSF27FunctionKey = 0xF71E,
    NSF28FunctionKey = 0xF71F,
    NSF29FunctionKey = 0xF720,
    NSF30FunctionKey = 0xF721,
    NSF31FunctionKey = 0xF722,
    NSF32FunctionKey = 0xF723,
    NSF33FunctionKey = 0xF724,
    NSF34FunctionKey = 0xF725,
    NSF35FunctionKey = 0xF726,
    NSInsertFunctionKey = 0xF727,
    NSDeleteFunctionKey = 0xF728,
    NSHomeFunctionKey = 0xF729,
    NSBeginFunctionKey = 0xF72A,
    NSEndFunctionKey = 0xF72B,
    NSPageUpFunctionKey = 0xF72C,
    NSPageDownFunctionKey = 0xF72D,
    NSPrintScreenFunctionKey = 0xF72E,
    NSScrollLockFunctionKey = 0xF72F,
    NSPauseFunctionKey = 0xF730,
    NSSysReqFunctionKey = 0xF731,
    NSBreakFunctionKey = 0xF732,
    NSResetFunctionKey = 0xF733,
    NSStopFunctionKey = 0xF734,
    NSMenuFunctionKey = 0xF735,
    NSUserFunctionKey = 0xF736,
    NSSystemFunctionKey = 0xF737,
    NSPrintFunctionKey = 0xF738,
    NSClearLineFunctionKey = 0xF739,
    NSClearDisplayFunctionKey = 0xF73A,
    NSInsertLineFunctionKey = 0xF73B,
    NSDeleteLineFunctionKey = 0xF73C,
    NSInsertCharFunctionKey = 0xF73D,
    NSDeleteCharFunctionKey = 0xF73E,
    NSPrevFunctionKey = 0xF73F,
    NSNextFunctionKey = 0xF740,
    NSSelectFunctionKey = 0xF741,
    NSExecuteFunctionKey = 0xF742,
    NSUndoFunctionKey = 0xF743,
    NSRedoFunctionKey = 0xF744,
    NSFindFunctionKey = 0xF745,
    NSHelpFunctionKey = 0xF746,
    NSModeSwitchFunctionKey = 0xF747,

    LeftMouseButton = 0xF748,
    RightMouseButton = 0xF749,
    MiddleMouseButton = 0xF74A,

    // OShiftKey, OControlKey, and OAltKey from Outlaws.h go here

    SpecialKeyCount = 0xF74E,
};

enum KeyMods {
    MOD_CTRL = 1<<18,
    MOD_ALT     = 1<<19,
    MOD_SHFT   = 1<<20
};

class KeyState {
    bool ascii[256];
    bool function[SpecialKeyCount - NSUpArrowFunctionKey];
    bool misc;

public:

    float2 cursorPosScreen;

    KeyState()
    {
        reset();
    }

    bool& operator[](int c)
    {
        if (0 <= c && c < 256)
            return ascii[c];
        if (NSUpArrowFunctionKey <= c && c < SpecialKeyCount)
            return function[c - NSUpArrowFunctionKey];
        else
            ASSERT(0);
        return misc;
    }

    bool operator[](int c) const
    {
        return (*const_cast<KeyState*>(this))[c];
    }

    void reset()
    {
        memset(ascii, false, sizeof(ascii));
        memset(function, false, sizeof(function));
        misc = false;
    }

    void cancelMouseDown()
    {
        (*this)[0 + LeftMouseButton] = false;
        (*this)[1 + LeftMouseButton] = false;
        (*this)[2 + LeftMouseButton] = false;
    }

    uint keyMods() const
    {
        uint mods = 0;
        if ((*this)[OControlKey])
            mods |= MOD_CTRL;
        if ((*this)[OShiftKey])
            mods |= MOD_SHFT;
        if ((*this)[OAltKey])
            mods |= MOD_ALT;
        return mods;
    }

    static KeyState &instance()
    {
        static KeyState v;
        return v;
    }

};

inline string keyToString(int key)
{
    switch (key) 
    {
    case LeftMouseButton: return "Left Mouse";
    case RightMouseButton: return "Right Mouse";
    case MiddleMouseButton: return "Middle Mouse";
    case NSUpArrowFunctionKey: return "Up";
    case NSDownArrowFunctionKey: return "Down";
    case NSRightArrowFunctionKey: return "Right";
    case NSLeftArrowFunctionKey: return "Left";
    case OShiftKey: return "Shift";
    case OControlKey: return "Control";
    case OAltKey: return "Alt";
    case '\t': return "Tab";
    case '\r': return "Enter";
    case ' ': return "Space";
    case NSPageUpFunctionKey: return "Page Up";
    case NSPageDownFunctionKey: return "Page Down";
    case NSHomeFunctionKey: return "Home";
    case NSEndFunctionKey: return "End";
    case NSDeleteFunctionKey: return "Delete";
    case NSBackspaceCharacter: return "Backspace";
    case EscapeCharacter: return "Escape";
    case NSF1FunctionKey: return "F1";
    case NSF2FunctionKey: return "F2";
    case NSF3FunctionKey: return "F3";
    case NSF4FunctionKey: return "F4";
    case NSF5FunctionKey: return "F5";
    case NSF6FunctionKey: return "F6";
    case NSF7FunctionKey: return "F7";
    case NSF8FunctionKey: return "F8";
    case NSF9FunctionKey: return "F9";
    case NSF10FunctionKey: return "F10";
    case NSF11FunctionKey: return "F11";
    case NSF12FunctionKey: return "F12";
    case 0: return " ";
    default:
        if (isprint(key))
            return str_format("%c", key);
        else
            return str_format("%#x", key);
    }
}


#endif // CORE_EVENT_H
