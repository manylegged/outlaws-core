//
// Event.cpp - Input tracking utils
// 

// Copyright (c) 2014-2015 Arthur Danskin
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


#include "StdAfx.h"
#include "Event.h"
#include "Unicode.h"

string Event::toString() const
{
    const string skey = keyToString(key);
    switch(type)
    {
    case KEY_DOWN:      return str_format("KEY_DOWN %s", skey.c_str());
    case KEY_UP:        return str_format("KEY_UP %s", skey.c_str());
    case MOUSE_DOWN:    return str_format("MOUSE_DOWN %d (%.f, %.f)", (int)key, pos.x, pos.y);
    case MOUSE_UP:      return str_format("MOUSE_UP %d (%.f, %.f)", (int)key, pos.x, pos.y);
    case MOUSE_DRAGGED: return str_format("MOUSE_DRAGGED %d (%.f, %.f)",(int)key, pos.x, pos.y);
    case MOUSE_MOVED:   return str_format("MOUSE_MOVED, (%.f, %.f)", pos.x, pos.y);
    case SCROLL_WHEEL:  return str_format("SCROLL_WHEEL %g", vel.y);
    case GAMEPAD_AXIS:  return str_format("GAMEPAD_AXIS %s %g", skey.c_str(), pos.y);
    case LOST_FOCUS:    return str_format("LOST_FOCUS");
    default:            return "unknown";
    }
}

bool Event::isMouse() const
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

bool Event::isKey() const
{
    switch (type) {
    case KEY_DOWN:
    case KEY_UP:
        return true;
    default:
        return false;
    }
}

bool& KeyState::operator[](int c)
{
    if (0 <= c && c < 256) {
        return ascii[c];
    } else if (NSUpArrowFunctionKey <= c && c < SpecialKeyMax)
        return function[c - NSUpArrowFunctionKey];
    else if (LeftMouseButton <= c && c < EventKeyMax) {
        return device[c - LeftMouseButton];
    } else {
        return misc[c];
    }
}


void KeyState::reset()
{
    memset(ascii, false, sizeof(ascii));
    memset(function, false, sizeof(function));
    memset(device, false, sizeof(device));
    misc.clear();
    foreach (float2& ax, gamepadAxis)
        ax = float2();
}

uint KeyState::keyMods() const
{
    uint mods = 0;
    if ((*this)[OControlKey]) mods |= MOD_CTRL;
    if ((*this)[OShiftKey])   mods |= MOD_SHFT;
    if ((*this)[OAltKey])     mods |= MOD_ALT;
    return mods;
}

Event KeyState::OnEvent(const Event* event)
{
    Event revent;
    KeyState &self = *this;
    switch (event->type)
    {
    case Event::KEY_DOWN:
    case Event::KEY_UP:
        // toggle right mouse on/off with control while holding left mouse
        if (self[LeftMouseButton] && event->key == OControlKey) {
            self[RightMouseButton] = (event->type == Event::KEY_DOWN);
        }
        self[event->rawkey] = (event->type == Event::KEY_DOWN);
        break;

    case Event::MOUSE_DOWN:
        self[event->key + LeftMouseButton] = true;
        cursorPosScreen = event->pos;
        break;

    case Event::MOUSE_UP:
        // we might control click, then normal up-click
        if (!self[event->key + LeftMouseButton] ||
            self[OControlKey])
        {
            self.cancelMouseDown();
        }
        self[event->key + LeftMouseButton] = false;
        cursorPosScreen = event->pos;
        break;

    case Event::MOUSE_MOVED:
    case Event::MOUSE_DRAGGED:
    {
        cursorPosScreen = event->pos;
        gamepadActive = false;
        break;
    }
    case Event::GAMEPAD_AXIS: 
    {
        float* val = NULL;
        switch (event->key)
        {
        case GamepadAxisLeftX:         val = &getAxis(GamepadAxisLeft).x; break;
        case GamepadAxisLeftY:         val = &getAxis(GamepadAxisLeft).y; break;
        case GamepadAxisRightX:        val = &getAxis(GamepadAxisRight).x; break;
        case GamepadAxisRightY:        val = &getAxis(GamepadAxisRight).y; break;
        case GamepadAxisTriggerLeftY:  val = &getAxis(GamepadAxisTriggerLeft).y; break;
        case GamepadAxisTriggerRightY: val = &getAxis(GamepadAxisTriggerRight).y; break;
        }
        if (val)
        {
            // create button up or down events when each axis toggles
            const bool isDown = *val == 0.f && event->pos.y != 0.f;
            const bool isUp   = *val != 0.f && event->pos.y == 0.f;
            if (isDown || isUp)
            {
                revent.type = isDown ? Event::KEY_DOWN : Event::KEY_UP;
                const int key = event->key;
                const bool isPlus = *val > 0.f || event->pos.y > 0.f;
                if (key == GamepadAxisLeftX && isPlus)        revent.key = GamepadLeftLeft;
                else if (key == GamepadAxisLeftX && !isPlus)  revent.key = GamepadLeftRight;
                else if (key == GamepadAxisLeftY && isPlus)   revent.key = GamepadLeftDown;
                else if (key == GamepadAxisLeftY && !isPlus)  revent.key = GamepadLeftUp;
                else if (key == GamepadAxisRightX && isPlus)  revent.key = GamepadRightLeft;
                else if (key == GamepadAxisRightX && !isPlus) revent.key = GamepadRightRight;
                else if (key == GamepadAxisRightY && isPlus)  revent.key = GamepadRightDown;
                else if (key == GamepadAxisRightY && !isPlus) revent.key = GamepadRightUp;
                else if (key == GamepadAxisTriggerLeftY)      revent.key = GamepadTriggerLeft;
                else if (key == GamepadAxisTriggerRightY)     revent.key = GamepadTriggerRight;
                revent.rawkey = revent.key;
            }
            *val = event->pos.y;
            if (abs(event->pos.y) > epsilon)
                gamepadActive = true;
        }
        break;
    }
    case Event::GAMEPAD_REMOVED:
    case Event::LOST_FOCUS:
        reset();
        fflush(NULL); // flush all open streams
        break;
    
    default:
        break;
    }
    return revent;
}

static string keyToUTF8(int key)
{
    if (key < 255) {
        return std::isprint(key) ? str_format("%c", key) : string();
    } else if ((NSUpArrowFunctionKey <= key && key < SpecialKeyMax) ||
               (LeftMouseButton <= key && key < EventKeyMax)) {
        return "";
    } else { 
        return UCS2_to_UTF8(key);
    }
}

static string rawKeyToString(int key)
{
    switch (key) 
    {
    case LeftMouseButton:          return "Left Mouse";
    case RightMouseButton:         return "Right Mouse";
    case MiddleMouseButton:        return "Middle Mouse";
    case MouseButtonFour:          return "Mouse4";
    case MouseButtonFive:          return "Mouse5";
    case NSUpArrowFunctionKey:     return "Up";
    case NSDownArrowFunctionKey:   return "Down";
    case NSRightArrowFunctionKey:  return "Right";
    case NSLeftArrowFunctionKey:   return "Left";
    case OShiftKey:                return "Shift";
    case OControlKey:              return "Control";
    case OAltKey:                  return "Alt";
    case '\t':                     return "Tab";
    case '\r':                     return "Enter";
    case ' ':                      return "Space";
    case NSPageUpFunctionKey:      return "Page Up";
    case NSPageDownFunctionKey:    return "Page Down";
    case NSHomeFunctionKey:        return "Home";
    case NSEndFunctionKey:         return "End";
    case NSDeleteFunctionKey:      return "Delete";
    case NSBackspaceCharacter:     return "Backspace";
    case EscapeCharacter:          return "Escape";
    case NSF1FunctionKey:          return "F1";
    case NSF2FunctionKey:          return "F2";
    case NSF3FunctionKey:          return "F3";
    case NSF4FunctionKey:          return "F4";
    case NSF5FunctionKey:          return "F5";
    case NSF6FunctionKey:          return "F6";
    case NSF7FunctionKey:          return "F7";
    case NSF8FunctionKey:          return "F8";
    case NSF9FunctionKey:          return "F9";
    case NSF10FunctionKey:         return "F10";
    case NSF11FunctionKey:         return "F11";
    case NSF12FunctionKey:         return "F12";
    case GamepadA:                 return "Gamepad A";
    case GamepadB:                 return "Gamepad B";
    case GamepadX:                 return "Gamepad X";
    case GamepadY:                 return "Gamepad Y";
    case GamepadBack:              return "Gamepad Back";
    case GamepadGuide:             return "Gamepad Guide";
    case GamepadStart:             return "Gamepad Start";
    case GamepadLeftStick:         return "Left Stick";
    case GamepadRightSitck:        return "Right Stick";
    case GamepadLeftShoulder:      return "Left Bumper";
    case GamepadRightShoulder:     return "Right Bumper";
    case GamepadDPadUp:            return "DPad Up";
    case GamepadDPadDown:          return "DPad Down";
    case GamepadDPadLeft:          return "DPad Left";
    case GamepadDPadRight:         return "DPad Right";
    case GamepadAxisLeftX:         return "Left Axis X";
    case GamepadAxisLeftY:         return "Left Axis Y";
    case GamepadAxisRightX:        return "Right Axis X";
    case GamepadAxisRightY:        return "Right Axis Y";
    case GamepadAxisTriggerLeftY:  return "Left Trigger Axis";
    case GamepadAxisTriggerRightY: return "Right Trigger Axis";
    case GamepadLeftUp:            return "Left Stick Up";
    case GamepadLeftDown:          return "Left Stick Down";
    case GamepadLeftLeft:          return "Left Stick Left";
    case GamepadLeftRight:         return "Left Stick Right";
    case GamepadRightUp:           return "Right Stick Up";
    case GamepadRightDown:         return "Right Stick Down";
    case GamepadRightLeft:         return "Right Stick Left";
    case GamepadRightRight:        return "Right Stick Right";
    case GamepadTriggerLeft:       return "Left Trigger";
    case GamepadTriggerRight:      return "Right Trigger";
    case Keypad0:                  return "Keypad 0";
    case Keypad1:                  return "Keypad 1";
    case Keypad2:                  return "Keypad 2";
    case Keypad3:                  return "Keypad 3";
    case Keypad4:                  return "Keypad 4";
    case Keypad5:                  return "Keypad 5";
    case Keypad6:                  return "Keypad 6";
    case Keypad7:                  return "Keypad 7";
    case Keypad8:                  return "Keypad 8";
    case Keypad9:                  return "Keypad 9";
    case KeyVolumeDown:            return "Volume Down";
    case KeyVolumeUp:              return "Volume Up";
    case KeyAudioNext:             return "Play Next";
    case KeyAudioPrev:             return "Play Prev";
    case KeyAudioPlay:             return "Play";
    case KeyAudioStop:             return "Stop";
    case KeyAudioMute:             return "Mute";
    case 0:                        return " ";
    default:                       return or_(keyToUTF8(key), str_format("%#x", key));
    }
}

string keyToString(int key)
{
    if (key == -1)
        return "<Invalid>";
    string str;
    if (key&MOD_CTRL) str += "Ctrl-";
    if (key&MOD_ALT)  str += "Alt-";
    if (key&MOD_SHFT) str += "Shft-";
    return str + rawKeyToString(key&~MOD_MASK);
}

string Event::toUTF8() const
{
    return keyToUTF8(key);
}

