//
// Event.cpp - Input tracking utils
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


#include "StdAfx.h"
#include "Event.h"

static DEFINE_CVAR(float, kGamepadKeyThreshold, 0.5f);
static DEFINE_CVAR(float, kScrollWheelDirection, 1);

Event::Event(const OLEvent &ev)
{
    type   = (Event::Type) ev.type; // hack...
    which  = ev.which;
    key    = ev.key;
    rawkey = chr_unshift(ev.key);
    pos    = float2(ev.x, ev.y);
    vel    = float2(ev.dx, ev.dy);

    if (isMouse() && !isBetween(key, 0, kMouseButtonCount-1))
        return;

    if (type == Event::SCROLL_WHEEL) {
        vel.y *= kScrollWheelDirection;
    }

    // mouse emulation
    switch (type) {
    case Event::TOUCH_BEGIN: type = Event::MOUSE_DOWN; break;
    case Event::TOUCH_ENDED: type = Event::MOUSE_UP; break;
    case Event::TOUCH_MOVED: type = Event::MOUSE_DRAGGED; break;
    default: break;
    }
}

string Event::toString() const
{
    const string skey = keyToString(key);
    string str;
    switch (type)
    {
    case KEY_DOWN:      str = str_format("KEY_DOWN %s (pad %d)", skey.c_str(), which); break;
    case KEY_UP:        str = str_format("KEY_UP %s", skey.c_str()); break;
    case MOUSE_DOWN:    str = str_format("MOUSE_DOWN %d (%.f, %.f)", (int)key, pos.x, pos.y); break;
    case MOUSE_UP:      str = str_format("MOUSE_UP %d (%.f, %.f)", (int)key, pos.x, pos.y); break;
    case MOUSE_DRAGGED: str = str_format("MOUSE_DRAGGED %d (%.f, %.f)",(int)key, pos.x, pos.y); break;
    case MOUSE_MOVED:   str = str_format("MOUSE_MOVED, (%.f, %.f)", pos.x, pos.y); break;
    case SCROLL_WHEEL:  str = str_format("SCROLL_WHEEL %g", vel.y); break;
    case GAMEPAD_AXIS:  str = str_format("GAMEPAD_AXIS %s %g (pad %d)", skey.c_str(), pos.y, which); break;
    case LOST_FOCUS:    str = str_format("LOST_FOCUS"); break;
    default:            str = "unknown"; break;
    }
    if (synthetic)
        str += " (synthetic)";
    return str;
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
        if (!nearZero(ax))
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
    switch (key)
    {
    case OShiftKey:
    case OControlKey:
    case OAltKey:
        return key;
    }
    const int mkey = key|keyMods();
    if (chr_unshift(key) != key)
    {
        return mkey&~MOD_SHFT;
    }
    return mkey;
}

static DEFINE_CVAR(bool, kGamepadButtonColors, false);

static string unicode_key(int color, int chr)
{
    string s = utf8_encode(chr);
    if (kGamepadButtonColors)
        s = str_format("^%c", ('0' + color)) + s + "^7";
    return s;
}

static const char* getGamepadKey(uint key)
{
    const KeyState::GamepadStyle style = KeyState::instance().getStyle();
    if (style == KeyState::DEFAULT)
    {
        switch (key) {
        case GamepadA:             return kGamepadButtonColors ? "^2A^7" : "A";
        case GamepadB:             return kGamepadButtonColors ? "^1B^7" : "B";
        case GamepadX:             return kGamepadButtonColors ? "^4X^7" : "X";
        case GamepadY:             return kGamepadButtonColors ? "^3Y^7" : "Y";
        case GamepadLeftShoulder:  return _("Left Bumper");
        case GamepadRightShoulder: return _("Right Bumper");
        case GamepadTriggerLeft:   return _("Left Trigger");
        case GamepadTriggerRight:  return _("Right Trigger");
        case GamepadBack:          return _("Back");
        }
    }
    else if (style == KeyState::PLAYSTATION)
    {
        static const string gamepadA = unicode_key(4, 0x2715); // ✕
        // static const string gamepadB = unicode_key(1, 0x25CB) + "^7"; // ○
        static const string gamepadB = unicode_key(1, 0x25EF); // ◯
        // static const string gamepadX = unicode_key(6, 0x25A1) + "^7"; // □
        static const string gamepadX = unicode_key(6, 0x25FB); // ◻
        static const string gamepadY = unicode_key(5, 0x25B3); // △
        switch (key) {
        case GamepadA:             return gamepadA.c_str();
        case GamepadB:             return gamepadB.c_str();
        case GamepadX:             return gamepadX.c_str();
        case GamepadY:             return gamepadY.c_str();
        case GamepadLeftShoulder:  return "L1";
        case GamepadRightShoulder: return "R1";
        case GamepadTriggerLeft:   return "L2";
        case GamepadTriggerRight:  return "R2";
        case GamepadBack:          return _("Share");
        }
    }
    return "";
}

const char* KeyState::stringNext() const
{
    return gamepadActive ? getGamepadKey(GamepadB) : _("ENTER/Click"); 
}

const char* KeyState::stringYes() const
{
    return gamepadActive ? getGamepadKey(GamepadA) : _("ENTER"); 
}

const char* KeyState::stringNo() const
{
    return gamepadActive ? getGamepadKey(GamepadB) : _("ESC"); 
}

const char* KeyState::stringDiscard() const
{
    return gamepadActive ? getGamepadKey(GamepadY) : _("DELETE"); 
}

const char* KeyState::stringStart() const
{
    return gamepadActive ? getGamepadKey(GamepadStart) : _("ENTER");
}


const char* KeyState::gamepadName() const
{
#if OL_HAS_SDL
    return OL_GetGamepadName(lastGamepad);
#else
    return NULL;
#endif
}

void KeyState::setLastGamepad(const Event *evt)
{
    const int prev = lastGamepad;
    lastGamepad = evt->which;
    if (prev != lastGamepad)
    {
        const char* name = gamepadName();
        lastStyle = (str_contains(name, "PS4") ||
                     str_contains(name, "PS3") ||
                     str_contains(name, "Playstation")) ? PLAYSTATION : DEFAULT;
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

static int gamepadAxis2Key(int key, int val)
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

static void gamepadAxis2Event(const Event *event, float last_val)
{
    // update thread processes events and processes synthetic events immediately
    // creating these synthetic key events from render thread results in duplicate key events
    if (event->synthetic || globals.isMainThread())
        return;
    
    const int last = sign_int(last_val, kGamepadKeyThreshold);
    const int current = sign_int(event->pos.y, kGamepadKeyThreshold);
    
    if (last == current)
        return;

    Event evt = *event;
    
    if (last != 0)
    {
        evt.type = Event::KEY_UP;
        evt.key = gamepadAxis2Key(event->key, last);
        pushEvent(&evt);
    }

    if (current != 0)
    {
        evt.type = Event::KEY_DOWN;
        evt.key = gamepadAxis2Key(event->key, current);
        pushEvent(&evt);
    }
}

void KeyState::OnEvent(const Event* event)
{
    if (event->isMouse())
    {
        cursorPosScreen = event->pos;
        if (!event->synthetic)
            gamepadHasRight = false;
    }
    
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
            setLastGamepad(event);
        }
        else if (!event->synthetic)
        {
            gamepadActive = false;
        }
        break;

    case Event::MOUSE_DOWN:
        self[event->key + LeftMouseButton] = true;
        break;

    case Event::MOUSE_UP:
        // we might control click, then normal up-click
        if (!self[event->key + LeftMouseButton] ||
            self[OControlKey])
        {
            cancelMouseDown();
        }
        self[event->key + LeftMouseButton] = false;
        break;

    case Event::GAMEPAD_AXIS: 
    {
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

        if (!nearZero(event->pos.y))
        {
            gamepadActive = true;
            if (cursorPosScreen == float2())
                cursorPosScreen = globals.windowSizePoints / 2.f;
            setLastGamepad(event);
            // cancelMouseDown();
            if (axis == GamepadAxisRight)
                gamepadHasRight = true;
        }

        GamepadInstance &gi = gamepads[event->which];
        const float last = gi.axis[axis][dim];
        gi.axis[axis][dim] = event->pos.y;
        gamepadAxis[axis][dim] = event->pos.y;
        gamepadAxis2Event(event, last);
        break;
    }
    case Event::GAMEPAD_ADDED:
        setLastGamepad(event);
        break;
    case Event::GAMEPAD_REMOVED:
        if (event->which == lastGamepad) {
            lastGamepad = -1;
            gamepadActive = false;
            gamepadHasRight = false;
        }
    case Event::LOST_FOCUS:
        reset();
        fflush(NULL); // flush all open streams
        break;
    
    default:
        break;
    }
}

KeyState & KeyState::instance(int which)
{
    static KeyState mn;
    static KeyState up;
    return (which == 0 || (which == -1 && globals.isMainThread())) ? mn : up;
}

static string keyToUTF8(int key)
{
    if (0 < key && key <= 255) {
        return std::isprint(key) ? str_format("%c", key) : string();
    } else if ((NSUpArrowFunctionKey <= key && key < SpecialKeyMax) ||
               (LeftMouseButton <= key && key < EventKeyMax)) {
        if (Keypad0 <= key && key <= Keypad9)
            return str_format("%c", '0' + (key - Keypad0));
        return string();
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
    case GamepadA:
    case GamepadB:
    case GamepadX:
    case GamepadY:
    case GamepadBack:
        return string(_("Gamepad")) + " " + getGamepadKey(key);
    case GamepadLeftShoulder:
    case GamepadRightShoulder:
    case GamepadTriggerLeft:
    case GamepadTriggerRight:
        return getGamepadKey(key);
    case GamepadStart:             return _("Gamepad Start");
    case GamepadGuide:             return _("Gamepad Guide");
    case GamepadLeftStick:         return _("Left Stick");
    case GamepadRightStick:        return _("Right Stick");
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
    case NSMenuFunctionKey:        return _("Menu");
    case KeyVolumeDown:            return _("Volume Down");
    case KeyVolumeUp:              return _("Volume Up");
    case KeyAudioNext:             return _("Play Next");
    case KeyAudioPrev:             return _("Play Prev");
    case KeyAudioPlay:             return _("Play");
    case KeyAudioStop:             return _("Stop");
    case KeyAudioMute:             return _("Mute");
    case 0:                        return " ";
    }

    const KeyState::GamepadStyle style = KeyState::instance().getStyle();
    if (style == KeyState::DEFAULT)
    {
        switch (key) {
        case GamepadA: return _("Gamepad A");
        case GamepadB: return _("Gamepad B");
        case GamepadX: return _("Gamepad X");
        case GamepadY: return _("Gamepad Y");
        }
    }
    else if (style == KeyState::PLAYSTATION)
    {
        static const string gamepadA = "Gamepad ^5" + utf8_encode(0x2715) + "^7"; // ✕
        static const string gamepadB = "Gamepad ^1" + utf8_encode(0x25CB) + "^7"; // ○
        static const string gamepadX = "Gamepad ^6" + utf8_encode(0x25A1) + "^7"; // □
        static const string gamepadY = "Gamepad ^5" + utf8_encode(0x25B3) + "^7"; // △
        switch (key) {
        case GamepadA: return gamepadA.c_str();
        case GamepadB: return gamepadB.c_str();
        case GamepadX: return gamepadX.c_str();
        case GamepadY: return gamepadY.c_str();
        }
    }

    return or_(keyToUTF8(key), str_format("%#x", key));
}

string keyToString(int key)
{
    if (key == -1)
        return "<Invalid>";
    string str;
    //. prefix for keybindings using Control
    if (key&MOD_CTRL) str += _("Ctrl-");
    //. prefix for keybindings using Alt/Option
    if (key&MOD_ALT)  str += _("Alt-");
    //. prefix for keybindings using Shift
    if (key&MOD_SHFT) str += _("Shft-");
    return str + rawKeyToString(key&~MOD_MASK);
}

string Event::toUTF8() const
{
    return keyToUTF8(key);
}

