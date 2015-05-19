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

string Event::toString() const
{
    const string skey = keyToString(key);
    switch(type)
    {
    case KEY_DOWN:      return str_format("KEY_DOWN %s (pad %d)", skey.c_str(), which);
    case KEY_UP:        return str_format("KEY_UP %s", skey.c_str());
    case MOUSE_DOWN:    return str_format("MOUSE_DOWN %d (%.f, %.f)", (int)key, pos.x, pos.y);
    case MOUSE_UP:      return str_format("MOUSE_UP %d (%.f, %.f)", (int)key, pos.x, pos.y);
    case MOUSE_DRAGGED: return str_format("MOUSE_DRAGGED %d (%.f, %.f)",(int)key, pos.x, pos.y);
    case MOUSE_MOVED:   return str_format("MOUSE_MOVED, (%.f, %.f)", pos.x, pos.y);
    case SCROLL_WHEEL:  return str_format("SCROLL_WHEEL %g", vel.y);
    case GAMEPAD_AXIS:  return str_format("GAMEPAD_AXIS %s %g (pad %d)", skey.c_str(), pos.y, which);
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

bool Event::isGamepad() const
{
    if (type == GAMEPAD_AXIS)
        return true;
    if (!isKey())
        return false;
    return GamepadA <= key && key <= GamepadAxisTriggerRightY;
}


KeyState::KeyState()
{
    reset();
}

bool KeyState::anyAxis() const
{
    foreach (const float2& ax, gamepadAxis)
        if (!isZero(ax))
            return true;
    return false;
}

void KeyState::cancelMouseDown()
{
    (*this)[0 + LeftMouseButton] = false;
    (*this)[1 + LeftMouseButton] = false;
    (*this)[2 + LeftMouseButton] = false;
}

int KeyState::getUpKey(const Event *evt) const
{
    return ((evt->type == Event::KEY_UP)   ? getModKey(evt->key) :
            (evt->type == Event::MOUSE_UP) ? getModKey(evt->key + LeftMouseButton) :
            0);
}

int KeyState::getDownKey(const Event *evt) const
{
    return ((evt->type == Event::KEY_DOWN)   ? getModKey(evt->key) :
            (evt->type == Event::MOUSE_DOWN) ? getModKey(evt->key + LeftMouseButton) :
            0);
}

int KeyState::getModKey(int key) const
{
    const int mkey = key|keyMods();
    if (chr_unshift(key) != key)
    {
        return mkey&~MOD_SHFT;
    }
    return mkey;
}

const char* KeyState::stringNext() const
{
    return gamepadActive ? "B" : _("ENTER/Click"); 
}

const char* KeyState::stringYes() const
{
    return gamepadActive ? "A" : _("ENTER"); 
}

const char* KeyState::stringNo() const
{
    return gamepadActive ? "B" : _("ESC"); 
}

const char* KeyState::stringDiscard() const
{
    return gamepadActive ? "Y" : _("DELETE"); 
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

static uint gamepadAxis2Key(uint key, float val)
{
    switch (key) {
    case GamepadAxisLeftX:         return val > 0 ? GamepadLeftRight : GamepadLeftLeft;
    case GamepadAxisLeftY:         return val > 0 ? GamepadLeftDown : GamepadLeftUp;
    case GamepadAxisRightX:        return val > 0 ? GamepadRightRight : GamepadRightLeft;
    case GamepadAxisRightY:        return val > 0 ? GamepadRightDown : GamepadRightUp;
    case GamepadAxisTriggerLeftY:  return GamepadTriggerLeft;
    case GamepadAxisTriggerRightY: return GamepadTriggerRight;
    }
    return 0;
}

static void gamepadAxis2Button(const Event *event, float before)
{
    const float current = event->pos.y;
    if ((before == current) ||
        (before > 0 && current > 0) ||
        (before < 0 && current < 0))
        return;

    Event evt = *event;
    
    if (before != 0.f)
    {
        evt.type = Event::KEY_UP;
        evt.key = gamepadAxis2Key(event->key, before);
        pushEvent(&evt);
    }

    if (current != 0.f)
    {
        evt.type = Event::KEY_DOWN;
        evt.key = gamepadAxis2Key(event->key, current);
        pushEvent(&evt);
    }
}

void KeyState::OnEvent(const Event* event)
{
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
        if (GamepadA <= event->key && event->key <= GamepadAxisTriggerRightY)
        {
            gamepads[event->which].buttons[event->key - GamepadA] = (event->type == Event::KEY_DOWN);
        }
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
        if (event->type == Event::MOUSE_MOVED)
            gamepadActive = false;
        break;
    }
    case Event::GAMEPAD_AXIS: 
    {
        if (abs(event->pos.y) > epsilon)
        {
            gamepadActive = true;
            lastGamepad = event->which;
        }

        GamepadAxis axis;
        int dim;
        switch (event->key)
        {
        case GamepadAxisLeftX:         axis = GamepadAxisLeft; dim = 0; break;
        case GamepadAxisLeftY:         axis = GamepadAxisLeft; dim = 1; break;
        case GamepadAxisRightX:        axis = GamepadAxisRight; dim = 0; break;
        case GamepadAxisRightY:        axis = GamepadAxisRight; dim = 1; break;
        case GamepadAxisTriggerLeftY:  axis = GamepadAxisTriggerLeft; dim = 1; break;
        case GamepadAxisTriggerRightY: axis = GamepadAxisTriggerRight; dim = 1; break;
        default: return;
        }

        GamepadInstance &gi = gamepads[event->which];
        const float before = gi.axis[axis][dim];
        gi.axis[axis][dim] = event->pos.y;
        gamepadAxis[axis][dim] = event->pos.y;
        gamepadAxis2Button(event, before);
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
}

static string keyToUTF8(int key)
{
    if (key < 255) {
        return std::isprint(key) ? str_format("%c", key) : string();
    } else if ((NSUpArrowFunctionKey <= key && key < SpecialKeyMax) ||
               (LeftMouseButton <= key && key < EventKeyMax)) {
        return "";
    } else { 
        return utf8_encode(key);
    }
}

static string rawKeyToString(int key)
{
    switch (key) 
    {
    case LeftMouseButton:          return _("Left Mouse");
    case RightMouseButton:         return _("Right Mouse");
    case MiddleMouseButton:        return _("Middle Mouse");
    case MouseButtonFour:          return _("Mouse4");
    case MouseButtonFive:          return _("Mouse5");
    case MouseButtonSix:           return _("Mouse6");
    case NSUpArrowFunctionKey:     return _("Up");
    case NSDownArrowFunctionKey:   return _("Down");
    case NSRightArrowFunctionKey:  return _("Right");
    case NSLeftArrowFunctionKey:   return _("Left");
    case OShiftKey:                return _("Shift");
    case OControlKey:              return _("Control");
    case OAltKey:                  return _("Alt");
    case '\t':                     return _("Tab");
    case '\r':                     return _("Enter");
    case ' ':                      return _("Space");
    case NSPageUpFunctionKey:      return _("Page Up");
    case NSPageDownFunctionKey:    return _("Page Down");
    case NSHomeFunctionKey:        return _("Home");
    case NSEndFunctionKey:         return _("End");
    case NSDeleteFunctionKey:      return _("Delete");
    case NSBackspaceCharacter:     return _("Backspace");
    case EscapeCharacter:          return _("Escape");
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
    case GamepadA:                 return _("Gamepad A");
    case GamepadB:                 return _("Gamepad B");
    case GamepadX:                 return _("Gamepad X");
    case GamepadY:                 return _("Gamepad Y");
    case GamepadBack:              return _("Gamepad Back");
    case GamepadGuide:             return _("Gamepad Guide");
    case GamepadStart:             return _("Gamepad Start");
    case GamepadLeftStick:         return _("Left Stick");
    case GamepadRightStick:        return _("Right Stick");
    case GamepadLeftShoulder:      return _("Left Bumper");
    case GamepadRightShoulder:     return _("Right Bumper");
    case GamepadDPadUp:            return _("DPad Up");
    case GamepadDPadDown:          return _("DPad Down");
    case GamepadDPadLeft:          return _("DPad Left");
    case GamepadDPadRight:         return _("DPad Right");
    case GamepadAxisLeftX:         return _("Left Axis X");
    case GamepadAxisLeftY:         return _("Left Axis Y");
    case GamepadAxisRightX:        return _("Right Axis X");
    case GamepadAxisRightY:        return _("Right Axis Y");
    case GamepadAxisTriggerLeftY:  return _("Left Trigger Axis");
    case GamepadAxisTriggerRightY: return _("Right Trigger Axis");
    case GamepadLeftUp:            return _("Left Stick Up");
    case GamepadLeftDown:          return _("Left Stick Down");
    case GamepadLeftLeft:          return _("Left Stick Left");
    case GamepadLeftRight:         return _("Left Stick Right");
    case GamepadRightUp:           return _("Right Stick Up");
    case GamepadRightDown:         return _("Right Stick Down");
    case GamepadRightLeft:         return _("Right Stick Left");
    case GamepadRightRight:        return _("Right Stick Right");
    case GamepadTriggerLeft:       return _("Left Trigger");
    case GamepadTriggerRight:      return _("Right Trigger");
    case Keypad0:                  return _("Keypad 0");
    case Keypad1:                  return _("Keypad 1");
    case Keypad2:                  return _("Keypad 2");
    case Keypad3:                  return _("Keypad 3");
    case Keypad4:                  return _("Keypad 4");
    case Keypad5:                  return _("Keypad 5");
    case Keypad6:                  return _("Keypad 6");
    case Keypad7:                  return _("Keypad 7");
    case Keypad8:                  return _("Keypad 8");
    case Keypad9:                  return _("Keypad 9");
    case KeyVolumeDown:            return _("Volume Down");
    case KeyVolumeUp:              return _("Volume Up");
    case KeyAudioNext:             return _("Play Next");
    case KeyAudioPrev:             return _("Play Prev");
    case KeyAudioPlay:             return _("Play");
    case KeyAudioStop:             return _("Stop");
    case KeyAudioMute:             return _("Mute");
    case 0:                        return " ";
    default:                       return or_(keyToUTF8(key), str_format("%#x", key));
    }
}

string keyToString(int key)
{
    if (key == -1)
        return "<Invalid>";
    string str;
    if (key&MOD_CTRL) str += _("Ctrl-");
    if (key&MOD_ALT)  str += _("Alt-");
    if (key&MOD_SHFT) str += _("Shft-");
    return str + rawKeyToString(key&~MOD_MASK);
}

string Event::toUTF8() const
{
    return keyToUTF8(key);
}

