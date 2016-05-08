//
// Event.h - Input tracking utils
// 

// Copyright (c) 2014-2016 Arthur Danskin
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

// special character codes, borrowed from OSX
// these are in the unicode private use area (E000—F8FF) so no worries about collisions
// 
enum : uint {
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

    SpecialKeyMax,

    LeftMouseButton = 0xF600,
    RightMouseButton,
    MiddleMouseButton,
    MouseButtonFour,
    MouseButtonFive,
    MouseButtonSix,

    // OShiftKey, OControlKey, and OAltKey from Outlaws.h go here
    
    // gamepad buttons
    GamepadA = 0xF620,
    GamepadB,
    GamepadX,
    GamepadY,
    GamepadBack,
    GamepadGuide,
    GamepadStart,
    GamepadLeftStick,
    GamepadRightStick,
    GamepadLeftShoulder,
    GamepadRightShoulder,
    GamepadDPadUp,
    GamepadDPadDown,
    GamepadDPadLeft,
    GamepadDPadRight,

    // synthetic gamepad axis buttons
    GamepadLeftUp,
    GamepadLeftDown,
    GamepadLeftLeft,
    GamepadLeftRight,
    GamepadRightUp,
    GamepadRightDown,
    GamepadRightLeft,
    GamepadRightRight,
    GamepadTriggerLeft,
    GamepadTriggerRight,

    // gamepad axes
    GamepadAxisLeftX,
    GamepadAxisLeftY,
    GamepadAxisRightX,
    GamepadAxisRightY,
    GamepadAxisTriggerLeftY,
    GamepadAxisTriggerRightY,

    Keypad0,
    Keypad1,
    Keypad2,
    Keypad3,
    Keypad4,
    Keypad5,
    Keypad6,
    Keypad7,
    Keypad8,
    Keypad9,

    KeyVolumeDown,
    KeyVolumeUp,
    KeyAudioNext,
    KeyAudioPrev,
    KeyAudioPlay,
    KeyAudioStop,
    KeyAudioMute,

    EventKeyMax
};

static const int kMouseButtonCount = MouseButtonSix - LeftMouseButton + 1;

struct Event {
    // !!! KEEP IN SYNC WITH Outlaws.h VERSION !!!
    enum Type { KEY_DOWN=0, KEY_UP, MOUSE_DOWN, MOUSE_UP, MOUSE_DRAGGED, 
                MOUSE_MOVED, SCROLL_WHEEL, LOST_FOCUS, GAINED_FOCUS,
                TOUCH_BEGIN, TOUCH_MOVED, TOUCH_STATIONARY, TOUCH_ENDED, TOUCH_CANCELLED,
                GAMEPAD_AXIS, GAMEPAD_ADDED, GAMEPAD_REMOVED,
                INVALID };
    Type   type = INVALID;
    int    key = 0;
    int    rawkey = 0;
    int    which = 0;           // which gamepad
    bool   synthetic = false;
    float2 pos;
    float2 vel;

    Event() {}
    Event(const OLEvent &ev);

    string toString() const;
    string toUTF8() const;
    bool isMouse() const;
    bool isKey() const;
    bool isGamepad() const;

    bool isKeyDown(const char* keys) const
    {
        return type == KEY_DOWN && keys && key < 255 && strchr(keys, key);
    }

    bool isKeyDown(int k) const
    {
        return type == KEY_DOWN && key == k;
    }

    bool isDown() const
    {
        return type == KEY_DOWN || type == MOUSE_DOWN;
    }

    bool isUp() const
    {
        return type == KEY_UP || type == MOUSE_UP;
    }

    bool isStart() const
    {
        return type == KEY_DOWN && (key == EscapeCharacter || 
                                    key == GamepadStart);
    }

    bool isEscape() const
    {
        return type == KEY_DOWN && (key == EscapeCharacter || 
                                    key == GamepadB ||
                                    key == GamepadBack);
    }

    bool isEnter() const
    {
        return type == KEY_DOWN && (key == '\r' || key == GamepadA);
    }
    
    bool isEnterUp() const
    {
        return type == KEY_UP && (key == '\r' || key == GamepadA);
    }

    bool isNext() const
    {
        return isEnter() || (type == MOUSE_DOWN && key == 0) || isEscape();
    }

};

enum KeyMods {
    MOD_CTRL = 1<<18,
    MOD_ALT  = 1<<19,
    MOD_SHFT = 1<<20,
    MOD_MASK = MOD_CTRL|MOD_ALT|MOD_SHFT,
};

enum GamepadAxis {
    GamepadAxisLeft = 0,
    GamepadAxisRight,
    GamepadAxisTriggerLeft,
    GamepadAxisTriggerRight,
    GamepadAxisCount
};

string keyToString(int key);

inline bool isGamepadKey(int key)
{
    return GamepadA <= key && key <= GamepadTriggerRight;
}

class KeyState {
public:
    enum GamepadStyle { DEFAULT, XBOX, PLAYSTATION };
private:
    bool                          ascii[256];
    bool                          function[SpecialKeyMax - NSUpArrowFunctionKey];
    bool                          device[EventKeyMax - LeftMouseButton];
    std::unordered_map<int, bool> misc;
    float2                        gamepadAxis[GamepadAxisCount];
    int                           lastGamepad = -1;
    GamepadStyle                  lastStyle   = DEFAULT;

    struct GamepadInstance {
        float2 axis[GamepadAxisCount];
        bool   buttons[GamepadAxisTriggerRightY - GamepadA];
    };
    std::unordered_map<int, GamepadInstance> gamepads;

    void setLastGamepad(const Event *evt);

public:

    float2 cursorPosScreen;
    bool   gamepadActive = false;
    bool   gamepadHasRight = false;

    KeyState();

    float2 &getAxis(GamepadAxis axis) { return gamepadAxis[axis]; }
    float2 getAxis(GamepadAxis axis) const { return gamepadAxis[axis]; }
    bool anyAxis() const;

    bool& operator[](int c);
    bool operator[](int c) const { return (*const_cast<KeyState*>(this))[c]; }

    void reset();
    void cancelMouseDown();
    uint keyMods() const;
    GamepadStyle getStyle() const { return lastStyle; }

    int getUpKey(const Event *evt) const;
    int getDownKey(const Event *evt) const;
    int getModKey(int key) const;

    const char* stringNext() const;
    const char* stringYes() const;
    const char* stringNo() const;
    const char* stringDiscard() const;
    const char* stringStart() const;

    const char* gamepadName() const;
    
    // update current button state
    void OnEvent(const Event* event);

    static KeyState &instance();

};


inline bool isKeyMod(int key)
{
    switch (key)
    {
    case OControlKey:
    case OShiftKey:
    case OAltKey:
        return true;
    default:
        return false;
    }
}

void pushEvent(const Event *ev);


#endif // CORE_EVENT_H
