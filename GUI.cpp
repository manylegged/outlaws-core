

//
// GUI.cpp - widget library
//

// Copyright (c) 2013-2015 Arthur Danskin
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
#include "GUI.h"

#include "Shaders.h"
#include "Unicode.h"
#include "Sound.h"

static DEFINE_CVAR(float, kScrollbarWidth, 25.f);
DEFINE_CVAR(float2, kButtonPad, float2(4.f));

bool ButtonBase::HandleEvent(const Event* event, bool* isActivate, bool* isPress)
{
    const float2 sz = 0.5f*size;

    bool handled = false;
    if (event->type == Event::KEY_DOWN || event->type == Event::KEY_UP)
    {
        if (active && event->key && (event->key == keys[0] || 
                                     event->key == keys[1] ||
                                     event->key == keys[2] || 
                                     event->key == keys[3]))
        {
            bool activate = !pressed && (event->type == Event::KEY_DOWN);
            handled       = true;
            pressed       = activate;
            if (isActivate)
                *isActivate = activate;
        }
    }
    else
    {
        if (event->isMouse())
            hovered = intersectPointRectangle(event->pos, position, sz);

        handled = visible && hovered && ((event->type == Event::MOUSE_DOWN) ||
                                         (event->type == Event::MOUSE_UP));
        // && (event->key == 0);

        const bool wasPressed = pressed;
        if (active && handled)
        {
            if (wasPressed && (event->type == Event::MOUSE_UP)) {
                if (isActivate)
                    *isActivate = true;
                pressed = false;
            } else if (!wasPressed && event->type == Event::MOUSE_DOWN) {
                if (isPress)
                    *isPress = true;
                pressed = true;
            }
        }
        else if (event->type == Event::MOUSE_MOVED || event->type == Event::LOST_FOCUS)
        {
            pressed = false;
        }
    }
    return handled;
}

bool ButtonBase::renderTooltip(const ShaderState &ss, const View& view, uint color, bool force) const
{
    if (tooltip.empty() || (!force && !hovered) || alpha < epsilon)
        return false;

    TextBox dat;
    dat.tSize = 11.f;
    dat.alpha = alpha;
    dat.fgColor = color;
    dat.bgColor = kGUIToolBg;
    dat.font = kMonoFont;
    dat.view = &view;
    dat.rad = size / 2.f;
    dat.Draw(ss, position, tooltip);
    return true;
}

void ButtonBase::renderSelected(const ShaderState &ss, uint bgcolor, uint linecolor, float alpha) const
{
    ShaderState s = ss;
    const float2 sz = 0.5f * size;
    const float2 p = position + float2(-sz.x - sz.y, 0);
    s.color32(bgcolor, alpha);
    ShaderUColor::instance().DrawTri(s, p + float2(0, sz.y), p + float2(sz.y / 2, 0), p + float2(0, -sz.y));
    s.translateZ(0.1f);
    s.color32(linecolor, alpha);
    ShaderUColor::instance().DrawLineTri(s, p + float2(0, sz.y), p + float2(sz.y / 2, 0), p + float2(0, -sz.y));
}

void ButtonBase::render(const ShaderState &ss, bool selected)
{
    if (!visible)
        return;

    DMesh::Handle h(theDMesh());
    renderButton(theDMesh(), selected);
    h.Draw(ss);
    
    renderContents(ss);
}

float2 Button::getTextSize() const
{
    const GLText* tx = GLText::get(textFont, GLText::getScaledSize(textSize), text);
    float2 sz = tx->getSize() + padding;

    if (subtext.size())
    {
        const GLText* stx = GLText::get(textFont, GLText::getScaledSize(subtextSize), text);
        sz.y += stx->getSize().y;
    }
    return sz;
}

void Button::renderButton(DMesh& mesh, bool selected)
{
    if (!visible)
        return;

    if (text.size() && !(style&S_FIXED))
    {
        const float2 sz = getTextSize();
        size.y = sz.y;
        size.x = max(max(size.x, sz.x), size.y * (float)kGoldenRatio);
    }

    if (style&S_3D)
    {
        DMesh::Scope s(mesh);
        const float z = -size.y * (pressed ? 1.f : hovered ? 0.75f : 0.5f);
        mesh.tri.translateZ(z);
        mesh.line.translateZ(z);
        if (style&S_BOX) {
            PushRect(&mesh.tri, &mesh.line, position, 0.5f * size, getBGColor(), getFGColor(selected), alpha);
        } else if (style&S_CORNERS) {
            PushButton(&mesh.tri, &mesh.line,  position, 0.5f * size, getBGColor(), getFGColor(selected), alpha);
        }
    }
    
    if (style&S_BOX) {
        PushRect(&mesh.tri, &mesh.line, position, 0.5f * size, getBGColor(), getFGColor(selected), alpha);
    } else if (style&S_CORNERS) {
        PushButton(&mesh.tri, &mesh.line,  position, 0.5f * size, getBGColor(), getFGColor(selected), alpha);
    }
}

void Button::renderContents(const ShaderState &s_)
{
    if (!visible)
        return;
    const uint tcolor = MultAlphaAXXX((!active) ? inactiveTextColor : textColor, alpha);
    const float2 pos = position + justY(subtext.size() ? size.y * (0.5f - (textSize / (subtextSize + textSize))) : 0.f);
    GLText::Put(s_, pos, subtext.size() ? GLText::CENTERED : GLText::MID_CENTERED,
                textFont, tcolor, textSize, text);
    
    if (subtext.size())
    {
        const uint stc = MultAlphaAXXX(subtextColor, alpha);
        GLText::Put(s_, pos , GLText::DOWN_CENTERED, textFont, stc, subtextSize, subtext);
    }
}

void TextInputBase::setText(const char* text, bool setSize)
{
    std::lock_guard<std::recursive_mutex> l(mutex);

    lines.clear();
    lines.push_back(string());
    string* line = &lines.back();
    uint longestLine = 0;
    if (text)
    {
        for (const char* ptr = text; *ptr != '\0'; ptr++) {
            if (*ptr == '\n') {
                lines.push_back(string());
                line = &lines.back();
            }
            else {
                line->push_back(*ptr);
                longestLine = max(longestLine, (uint)line->size());
            }
        }
    }
    cursor = int2(lines[lines.size()-1].size(), lines.size()-1);

    if (setSize)
    {
        sizeChars = int2(longestLine + 1, lines.size());
    }
}

void TextInputBase::setLines(const vector<string> &lns)
{
    std::lock_guard<std::recursive_mutex> l(mutex);
    lines.clear();
    vec_extend(lines, lns);

    cursor = int2(lines[lines.size()-1].size(), lines.size()-1);
}

static void cursor_move_utf8(const string& line, int2& cursor, int adjust)
{
    cursor.x += adjust;
    while (0 <= cursor.x && cursor.x < line.size() && utf8_iscont(line[cursor.x]))
        cursor.x += adjust;
}

static bool forward_char(int2& cursor, deque<string> &lines, int offset)
{
    if (offset == -1)
    {
        if (cursor.x == 0) {
            if (cursor.y <= 0)
                return false;

            cursor.y--;
            cursor.x = (int)lines[cursor.y].size();
        } else {
            cursor_move_utf8(lines[cursor.y], cursor, -1);
        }
    }
    else if (offset == 1)
    {
        if (cursor.x == lines[cursor.y].size()) {
            if (cursor.y >= lines.size()-1)
                return false;
            cursor.y++;
            cursor.x = 0;
        } else
            cursor_move_utf8(lines[cursor.y], cursor, +1);
    }
    return true;
}

static void forward_when(int2& cursor, deque<string> &lines, int offset, int (*pred)(int))
{
    forward_char(cursor, lines, offset);
    while (cursor.y < lines.size() &&
           (cursor.x >= lines[cursor.y].size() ||
            pred(lines[cursor.y][cursor.x])) &&
           forward_char(cursor, lines, offset));
}

static bool delete_char(int2& cursor, deque<string> &lines)
{
    if (0 > cursor.y || cursor.y >= lines.size()) {
        return '\0';
    } else if (cursor.x > 0) {
        string &line = lines[cursor.y];
        cursor_move_utf8(line, cursor, -1);
        line = utf8_erase(line, cursor.x, 1);
        return true;
    } else if (cursor.y > 0) {
        int nx = lines[cursor.y-1].size();
        lines[cursor.y - 1].append(lines[cursor.y]);
        lines.erase(lines.begin() + cursor.y);
        cursor.y--;
        cursor.x = nx;
        return true;
    } else {
        return false;
    }
}

static void delete_region(int2& cursor, deque<string> &lines, int2 mark)
{
    if (mark.y < cursor.y || (mark.y == cursor.y && mark.x < cursor.x))
        swap(cursor, mark);
    
    while (mark != cursor && delete_char(mark, lines));
}

bool TextInputBase::HandleEvent(const Event* event, bool *textChanged)
{
    active = forceActive || 
             intersectPointRectangle(KeyState::instance().cursorPosScreen, position, 0.5f * size);
    hovered = active;

    if (active && event->type == Event::SCROLL_WHEEL)
    {
        startChars.y += ceil_int(-event->vel.y);
        startChars.y = clamp(startChars.y, 0, max(0, (int)lines.size() - sizeChars.y));
        return true;
    }
    
    if (locked)
        return false;

    if (textChanged)
        *textChanged = false;

    if (active && event->type == Event::KEY_DOWN)
    {
        std::lock_guard<std::recursive_mutex> l(mutex);

        cursor.y = clamp(cursor.y, 0, lines.size());
        cursor.x = clamp(cursor.x, 0, lines[cursor.y].size());

        switch (KeyState::instance().keyMods() | event->key)
        {
        case MOD_CTRL|'b': forward_char(cursor, lines, -1); break;
        case MOD_CTRL|'f': forward_char(cursor, lines, +1); break;
        case MOD_CTRL|'p': cursor.y--; break;
        case MOD_CTRL|'n': cursor.y++; break;
        case MOD_CTRL|'a': cursor.x = 0; break;
        case MOD_CTRL|'e': cursor.x = lines[cursor.y].size(); break;
        case MOD_CTRL|'k':
            OL_WriteClipboard(lines[cursor.y].substr(cursor.x).c_str());
            if (cursor.x == lines[cursor.y].size() && cursor.y < lines.size()-1) {
                lines[cursor.y].append(lines[cursor.y+1]);
                lines.erase(lines.begin() + cursor.y + 1);
            } else
                lines[cursor.y].erase(cursor.x); 
            break;
        case MOD_CTRL|'v':
        case MOD_CTRL|'y':
            insertText(OL_ReadClipboard());
            if (textChanged)
                *textChanged = true;
            break;
        case MOD_ALT|NSRightArrowFunctionKey:
        case MOD_ALT|'f': forward_when(cursor, lines, +1, isalnum); break;
        case MOD_ALT|NSLeftArrowFunctionKey:
        case MOD_ALT|'b': forward_when(cursor, lines, -1, isalnum); break;
            
        case MOD_ALT|NSBackspaceCharacter: {
            const int2 scursor = cursor;
            forward_when(cursor, lines, -1, isalnum);
            delete_region(cursor, lines, scursor);
            if (textChanged)
                *textChanged = true;
            break;
        }
        case MOD_ALT|'d': {
            const int2 scursor = cursor;
            forward_when(cursor, lines, 1, isalnum);
            delete_region(cursor, lines, scursor);
            if (textChanged)
                *textChanged = true;
            break;
        }
        case MOD_ALT|'m':
            cursor.x = 0;
            forward_when(cursor, lines, +1, isspace);
            break;
        case NSLeftArrowFunctionKey:  forward_char(cursor, lines, -1); break;
        case NSRightArrowFunctionKey: forward_char(cursor, lines, +1); break;
        case NSUpArrowFunctionKey:    cursor.y = max(0, cursor.y-1); break;
        case NSDownArrowFunctionKey:  cursor.y = min((int)lines.size()-1, cursor.y+1); break;
        case NSHomeFunctionKey:       cursor.x = 0; break;
        case NSEndFunctionKey:        cursor.x = lines[cursor.y].size(); break;
        case MOD_CTRL|'d':
        case NSDeleteFunctionKey:
            if (!forward_char(cursor, lines, +1))
                break;
            // fallthrough!
        case NSBackspaceCharacter:
            delete_char(cursor, lines);
            if (textChanged)
                *textChanged = true;
            break;
        case '\r':
        {
            if (fixedSize && lines.size() >= sizeChars.y)
                return false;
            string s = utf8_substr(lines[cursor.y], cursor.x);
            lines[cursor.y] = utf8_erase(lines[cursor.y], cursor.x);
            lines.insert(lines.begin() + cursor.y + 1, s);
            if (textChanged)
                *textChanged = true;
            cursor.y++;
            cursor.x = 0;
            break;
        }
        case NSPageUpFunctionKey: 
            startChars.y -= sizeChars.y;
            startChars.y = clamp(startChars.y, 0, max(0, (int)lines.size() - sizeChars.y));
            return true;
        case NSPageDownFunctionKey:
            startChars.y += sizeChars.y;
            startChars.y = clamp(startChars.y, 0, max(0, (int)lines.size() - sizeChars.y));
            return true;
        default:
        {
            string str = event->toUTF8();
            if (str.empty())
                return false;
            ASSERT(cursor.x == utf8_advance(lines[cursor.y], cursor.x));
            lines[cursor.y].insert(cursor.x, str);
            cursor.x += str.size();
            if (textChanged)
                *textChanged = true;
            break;
        }
        }

        if (lines.size() == 0)
            lines.push_back("");
        
        if (cursor.y >= lines.size()) {
            cursor.x = lines.back().size();
            cursor.y = lines.size()-1;
        }
        cursor.y = clamp(cursor.y, 0, (int)lines.size()-1);
        cursor.x = clamp(cursor.x, 0, lines[cursor.y].size());
        scrollForInput();
            
        return true;
    }

    return false;
}

void TextInputBase::popText(int chars)
{
    std::lock_guard<std::recursive_mutex> l(mutex);
    while (chars && lines.size()) 
    {
        string &str = lines.back();
        int remove = min(chars, (int) str.size());
        str.resize(str.size() - remove);
        chars -= remove;
        if (str.size() == 0)
            lines.pop_back();
    }
}

void TextInputBase::pushText(const char *txt, int linesback)
{
    vector<string> nlines = str_split('\n', str_word_wrap(txt, sizeChars.x));
    
    std::lock_guard<std::recursive_mutex> l(mutex);

    const int pt = lines.size() - linesback;
    lines.insert(lines.end() - min(linesback, (int)lines.size()), nlines.begin(), nlines.end());

    if (cursor.y >= pt)
        cursor.y += nlines.size();
    // cursor.x = lines[cursor.y].size();
    scrollForInput();
}

void TextInputBase::insertText(const char *txt)
{
    if (!txt)
        return;
    vector<string> nlines = str_split('\n', txt);
    if (nlines.size() == 0)
        return;

    std::lock_guard<std::recursive_mutex> l(mutex);
    lines[cursor.y].insert(cursor.x, nlines[0]);
    cursor.x += nlines[0].size();

    lines.insert(lines.begin() + cursor.y + 1, nlines.begin() + 1, nlines.end());
    cursor.y += nlines.size()-1;
    
    scrollForInput();
}

float2 TextInputBase::getCharSize() const
{
    float2 size = FontStats::get(kMonoFont, GLText::getScaledSize(textSize)).charMaxSize;
    return size;
}


void TextInputBase::render(const ShaderState &s_)
{
    std::lock_guard<std::recursive_mutex> l(mutex);

    ShaderState s = s_;

    startChars.x = 0;
    
    const int2 start     = startChars;
    const int  drawLines = min((int)lines.size() - start.y, sizeChars.y);

    float2 longestPointSize(0);
    int    longestChars     = 0;
    for (int i=start.y; i<(start.y + drawLines); i++)
    {
        const GLText* tx = GLText::get(kMonoFont, GLText::getScaledSize(textSize), lines[i].c_str());
        if (tx->getSize().x > longestPointSize.x) {
            longestPointSize = tx->getSize();
            longestChars     = lines[i].size();
        }
    }
    const float charHeight = getCharSize().y;
    sizeChars.x = max(sizeChars.x, (int)longestChars + 1);
        
    //sizePoints.x = max(sizePoints.x, 2.f * kPadDist + longestPointSize.x);
    size.y = charHeight * sizeChars.y + kPadDist;

    const float2 sz = 0.5f * size;

    {
        DMesh::Handle h(theDMesh());
        h.mp.tri.color32(active ? activeBGColor : defaultBGColor, alpha);
        if (h.mp.tri.cur().color&ALPHA_OPAQUE)
            h.mp.tri.PushRect(position, sz);

        h.mp.line.color32(active ? activeLineColor : defaultLineColor, alpha);
        if (h.mp.line.cur().color&ALPHA_OPAQUE)
            h.mp.line.PushRect(position, sz);

        if (lines.size() > sizeChars.y && defaultLineColor)
        {
            scrollbar.size = float2(kScrollbarWidth, size.y - kButtonPad.y);
            scrollbar.position = position + justX(size/2.f) - justX(scrollbar.size.x/2.f + kButtonPad.x);
            scrollbar.alpha = alpha;
            scrollbar.first = startChars.y;
            scrollbar.sfirst = scrollbar.first;
            scrollbar.lines = sizeChars.y;
            scrollbar.steps = lines.size();
            scrollbar.defaultBGColor = defaultBGColor;
            scrollbar.hoveredFGColor = activeLineColor;
            scrollbar.defaultFGColor = defaultLineColor;
            scrollbar.render(h.mp);
        }

        h.Draw(s);
    }

    s.translate(position);

    s.translate(float2(-sz.x + kPadDist, sz.y - charHeight - kPadDist));
    s.color32(textColor, alpha);
        
    for (int i=start.y; i<(start.y + drawLines); i++)
    {
//            const string txt = lines[i].substr(start,
//                                               min(lines[i].size(), start.x + sizeChars.x));
        const GLText* tx = GLText::get(kMonoFont, GLText::getScaledSize(textSize), lines[i].c_str());

        if (lines[i].size())
        {
            tx->render(&s);
        }
        
        // draw cursor
        if (active && cursor.y == i) {
            ShaderState  s1    = s;
            const float  spos = tx->getCharStart(cursor.x);
            const float2 size  = tx->getCharSize(cursor.x);
            s1.translate(float2(spos, 0));
            s1.color(textColor, alpha);
            s1.translateZ(1.f);
            ShaderUColor::instance().DrawRectCorners(s1, float2(0), float2(size.x, charHeight));
            if (cursor.x < lines[i].size()) {
                GLText::Put(s1, float2(0.f), GLText::LEFT, kMonoFont,
                            ALPHA_OPAQUE|GetContrastWhiteBlack(textColor),
                            textSize, utf8_substr(lines[i], cursor.x, 1));
            }
        }
        
        s.translate(float2(0.f, -charHeight));
    }
}

TextInputCommandLine::TextInputCommandLine()
{
    sizeChars.y = 10;

    registerCommand(cmd_help, comp_help, this, "help", "[command]: list help for specified command, or all commands if unspecified");
    registerCommand(cmd_find, comp_help, this, "find", "[string]: list commands matching search");
    setLineText("");
}

void TextInputCommandLine::saveHistory(const char *fname)
{
    string str = str_join("\n", commandHistory);
    int status = OL_SaveFile(fname, str.c_str(), str.size());
    ReportMessagef("Wrote %d lines of console history to '%s': %s",
                   (int) commandHistory.size(), fname, status ? "OK" : "FAILED");
}

void TextInputCommandLine::loadHistory(const char *fname)
{
    const char* data = OL_LoadFile(fname);
    if (data) {
        string sdat = data;
#if WIN32
        sdat = str_replace(sdat, "\r", "");
#endif
        commandHistory = str_split('\n', sdat);
    }
    historyIndex = commandHistory.size();
}

vector<string> TextInputCommandLine::comp_help(void* data, const char* name, const char* args)
{
    vector<string> options;
    TextInputCommandLine *self = (TextInputCommandLine*) data;
    for (std::map<string, Command>::iterator it=self->commands.begin(), end=self->commands.end(); it != end; ++it)
    {
        options.push_back(it->first);
    }
    return options;
}

string TextInputCommandLine::cmd_help(void* data, const char* name, const char* args) 
{
    TextInputCommandLine* self = (TextInputCommandLine*) data;
        
    const string arg = str_strip(args);
    vector<string> helps = self->completeCommand(arg);
    if (helps.size() == 0)
    {
        for (std::map<string, Command>::iterator it=self->commands.begin(), end=self->commands.end(); it != end; ++it)
            helps.push_back(it->first);
    }
    string ss;
    foreach (const string &cmd, helps)
        ss += "^2" + cmd + "^7 " + self->commands[cmd].description + "\n";
    ss.pop_back();
    return ss;
}

string TextInputCommandLine::cmd_find(void* data, const char* name, const char* args)
{
    TextInputCommandLine* self = (TextInputCommandLine*) data;
        
    const string arg = str_strip(args);
    string ss;
    int count = 0;
    foreach (const auto& x, self->commands) {
        if (str_contains(x.first, arg) || str_contains(x.second.description, arg))
        {
            ss += "^2" + x.first + "^7 " + x.second.description + "\n";
            count++;
        }
    }
    if (count) {
        ss.pop_back();
    } else {
        ss = str_format("No commands matching '%s'", arg.c_str());
    }
    return ss;
}

void TextInputCommandLine::pushCmdOutput(const char *format, ...)
{
    va_list vl;
    va_start(vl, format);
    string txt = str_vformat(format, vl);
    va_end(vl);
    vector<string> nlines = str_split('\n', str_word_wrap(txt, sizeChars.x));

    std::lock_guard<std::recursive_mutex> l(mutex);
    lines.insert(lines.end(), nlines.begin(), nlines.end());
    lines.push_back(prompt);

    cursor.y = (int)lines.size() - 1;
    cursor.x = lines[cursor.y].size();
    scrollForInput();
}

vector<string> TextInputCommandLine::completeCommand(string cmd) const
{
    vector<string> possible;
    foreach (const auto& x, commands) {
        if (x.first.size() >= cmd.size() && x.first.substr(0, cmd.size()) == cmd)
            possible.push_back(x.first);
    }
    return possible;
}

bool TextInputCommandLine::doCommand(const string& line)
{
    pushHistory(line);
    const vector<string> expressions = str_split_quoted(';', line);
    foreach (const string &expr, expressions)
    {
        vector<string> args = str_split_quoted(' ', str_strip(expr));
        if (!args.size())
            return false;
        string cmd = str_tolower(args[0]);
        if (args.size() == 0) {
            pushCmdOutput("");
            return false;
        }

        Command *c = NULL;
        if (map_contains(commands, cmd)) {
            c = &commands[cmd];
        } else {
            const vector<string> possible = completeCommand(cmd);
            if (possible.size() == 1) {
                c = &commands[possible[0]];
            } else {
                pushCmdOutput("No such command '%s'%s", cmd.c_str(),
                              possible.size() ? str_format(", did you mean %s?", 
                                                           str_join(", ", possible).c_str()).c_str() : "");
                return false;
            }
        }

        DPRINT(CONSOLE, ("'%s'", expr.c_str()));
        
        const string argstr = str_join(" ", args.begin() + 1, args.end());
        const string ot = c->func(c->data, cmd.c_str(), argstr.c_str());

        DPRINT(CONSOLE, ("-> '%s'", ot.c_str()));
        
        const vector<string> nlines = str_split('\n', str_word_wrap(ot, sizeChars.x));
        lines.insert(lines.end(), nlines.begin(), nlines.end());
    }
    pushCmdOutput("");      // prompt
    return true;
}

bool TextInputCommandLine::pushCommand(const string& line)
{
    setLineText(line.c_str());
    return doCommand(line);
}

const TextInputCommandLine::Command *TextInputCommandLine::getCommand(const string &abbrev) const
{
    const string cmd = str_tolower(abbrev);
    if (map_contains(commands, cmd)) {
        return map_addr(commands, cmd);
    } else {
        vector<string> possible;
        foreach (const auto& x, commands) {
            if (x.first.size() > cmd.size() && x.first.substr(0, cmd.size()) == cmd)
                possible.push_back(x.first);
        }
        if (possible.size() == 1) {
            return map_addr(commands, possible[0]);
        }
        return NULL;
    }
}

bool TextInputCommandLine::HandleEvent(const Event* event, bool *textChanged)
{
    if (textChanged)
        *textChanged = false;

    if (active && event->type == Event::KEY_DOWN)
    {
        switch (KeyState::instance().keyMods() | event->key)
        {
        case NSUpArrowFunctionKey:
        case NSDownArrowFunctionKey:
            if (commandHistory.size())
            {
                lastSearch = "";
                const int delta = (event->key == NSUpArrowFunctionKey) ? -1 : 1;
                historyIndex = (historyIndex + delta) % commandHistory.size();
                setLineText(commandHistory[historyIndex].c_str());
            }
            return true;
        case '\r':
        {
            lastSearch = "";
            doCommand(getLineText());
            return true;
        }
        case MOD_CTRL|'r':
        case MOD_CTRL|'s':
        case MOD_ALT|'p':
        case MOD_ALT|'n':
        {
            int end = 0;
            int delta = 0;
            if (event->key == 'p' || event->key == 'r') {
                end = 0;
                delta = -1;
            } else if (event->key == 'n' ||event->key == 's') {
                end = commandHistory.size();
                delta = 1;
            }

            if (lastSearch.empty())
                lastSearch = getLineText();
            if (lastSearch.empty())
                return true;

            for (int i=historyIndex; i != end; i += delta)
            {
                if (i != historyIndex &&
                    commandHistory[i].size() >= lastSearch.size() &&
                    commandHistory[i].substr(0, lastSearch.size()) == lastSearch)
                {
                    historyIndex = i;
                    setLineText(commandHistory[i].c_str());
                    break;
                }
            }
            return true;
        }
        case '\t':
        {
            lastSearch = "";
            string line = getLineText();
            int start = line.rfind(' ');
            vector<string> options;
            if (start > 0)
            {
                vector<string> args = str_split(' ', str_strip(line));
                if (args.size() >= 1)
                {
                    const Command *cmd = getCommand(args[0]);
                    if (cmd && cmd->comp)
                    {
                        const string argstr = str_join(" ", args.begin() + 1, args.end());
                        const string largs = str_tolower(argstr);
                        options = cmd->comp(cmd->data, cmd->name.c_str(), argstr.c_str());
                        for (int i=0; i<options.size(); ) {
                            vec_pop_increment(
                                options, i, str_tolower(options[i].substr(0, largs.size())) != largs);
                        }
                        line = cmd->name + " " + argstr;
                        start = argstr.size();
                    }
                }
            }
            else
            {
                for (std::map<string, Command>::iterator it=commands.begin(), end=commands.end(); it != end; ++it)
                {
                    const string lline = str_tolower(line);
                    if (str_tolower(it->first.substr(0, line.size())) == lline)
                        options.push_back(it->second.name);
                }
                start = line.size();
            }

            if (options.empty()) {
                pushText("No completions");
            } else {
                bool done = false;
                const string oline = line;
                line.resize(line.size() - start); // update case
                line += options[0].substr(0, start);
                for (int i=start; ; i++) {
                    const char c = options[0][i];
                    for (uint j=0; j<options.size(); j++) {
                        if (i == options[j].size() || options[j][i] != c) {
                            done = true;
                            break;
                        }
                    }
                    if (done)
                        break;
                    else
                        line += c;
                }
                
                if (options.size() > 1 && oline.size() == line.size())
                {
                    pushText((prompt + oline).c_str());
                    pushText(str_join(" ", options).c_str());
                }
                setLineText(line.c_str());
                if (textChanged)
                    *textChanged = true;
            }
            return true;
        }
        default:
            break;
        }
    }

    bool handled = TextInputBase::HandleEvent(event, textChanged);

    if (cursor.y >= lines.size()) {
        cursor.x = lines.back().size();
    } else {
        cursor.x = clamp(cursor.x, prompt.size(), lines.back().size());
    }
        
    cursor.y = (int)lines.size()-1;

    string& line = lines[cursor.y];
    if (line.size() < prompt.size()) {
        setLineText("");
    } else if (!str_startswith(line, prompt)) {
        for (int i=0; i<prompt.size(); i++) {
            if (line[i] != prompt[i])
                line.insert(i, 1, prompt[i]);
        }
    }

    if (handled)
        lastSearch = "";
        
    return handled;
}


void ContextMenuBase::render(ShaderState *s_)
{
    ShaderState s = *s_;

    // calculate size
    sizePoints.x = 0;
    sizePoints.y = 0;
    for (uint i=0; i<lines.size(); i++)
    {
        const GLText* tx = GLText::get(kDefaultFont, GLText::getScaledSize(textSize), lines[i].c_str());
        sizePoints.x = max(sizePoints.x, tx->getSize().x);
        sizePoints.y = lines.size() * tx->getSize().y;
    }
    {
        const GLText* tx = GLText::get(kDefaultFont, GLText::getScaledSize(textSize), title.c_str());
        titleSizePoints = tx->getSize();
        sizePoints.x = max(sizePoints.x, titleSizePoints.x);
        sizePoints.y += titleSizePoints.y;
    }
    hovered = !active ? -1 : getHoverSelection(KeyState::instance().cursorPosScreen);

    {
        const float2 sz = 0.5f * getMenuSizePoints();
        const float2 cr = getCenterPos();

        ShaderState s1 = s;
        s1.translate(cr);

        ShaderUColor::instance().DrawColorRect(s1, defaultBGColor, sz);
        const uint lineColor = (hovered >= 0) ? hoveredLineColor : defaultLineColor;
        ShaderUColor::instance().DrawColorLineRect(s1, lineColor, sz);
    }
        
    GLText::Put(s, position, GLText::LEFT, titleTextColor, textSize, title);

    if (lines.empty())
        return;

    const float textHeight = getMenuSizePoints().y / lines.size();

    for (uint i=0; i<lines.size(); i++)
    {
        s.translate(float2(0.f, -textHeight));

        // highlight hovered element
        if (hovered == i && (hoveredBGColor&ALPHA_OPAQUE)) {
            s.color32(hoveredBGColor);
            ShaderUColor::instance().DrawRectCorners(s, position, position + float2(sizePoints.x, textHeight));
        }

        const uint color = (selected == i) ? selectedTextColor : textColor;
        GLText::Put(s, position, GLText::LEFT, color, textSize, lines[i]);
    }
}

bool OptionButtons::HandleEvent(const Event* event, int* butActivate, int* butPress)
{
    Event ev = *event;
    ev.pos -= position;

    bool handled = false;
        
    for (int i=0; i<buttons.size(); i++)
    {
        bool isActivate = false;
        bool isPress    = false;
        if (buttons[i].HandleEvent(&ev, &isActivate, &isPress))
        {
            if (isActivate && butActivate)
                *butActivate = isActivate;
            if (isPress && butPress)
                *butPress = isPress;
            if (isPress)
                selected = i;
            handled = true;
            break;
        }
    }
    for (int j=0; j<buttons.size(); j++)
        buttons[j].pressed = (selected == j);
    return handled;
}

void OptionButtons::render(ShaderState *s_, const View& view)
{
    ShaderState ss = *s_;
    ss.translate(position);

    size = float2(0.f);
    float2 maxsize;
    foreach (Button& but, buttons) {
        maxsize = max(maxsize, but.size);
        size    = max(size, abs(but.position) + 0.5f * but.size);        
    }

    foreach (Button& but, buttons) {
        but.size = maxsize; // buttons are all the same size
        but.render(ss);
    }

    foreach (Button& but, buttons) {
        but.renderTooltip(ss, view, but.textColor);
    }

#if 0
    // for debugging - draw the bounding rectangle
    if (buttons.size()) {
        ss.color32(buttons[0].defaultLineColor);
        ShaderUColor::instance().DrawLineRect(ss, size);
    }
#endif
}

bool OptionSlider::HandleEvent(const Event* event, bool *valueChanged)
{
    if (!event->isMouse() || !active)
        return false;
    const float2 sz = 0.5f*size;
    
    hovered = pressed || intersectPointRectangle(event->pos, position, sz);
    pressed = (hovered && event->type == Event::MOUSE_DOWN) ||
              (!isDiscrete() && pressed && event->type == Event::MOUSE_DRAGGED);
    
    const bool handled = pressed || (hovered && event->isMouse());

    const int lasthval = hoveredValue;
    hoveredValue = hovered ? (isBinary() ? !value : floatToValue(((event->pos.x - position.x) / size.x) + 0.5f)) : -1;
    if (isDiscrete() && lasthval != hoveredValue)
        globals.sound->OnButtonHover();

    if (pressed) {
        if (valueChanged && (value != hoveredValue)) {
            if (isDiscrete())
                globals.sound->OnButtonPress();
            *valueChanged = true;
        }
        value = hoveredValue;
    }

    return handled;
}

void OptionSlider::render(const ShaderState &s_)
{
    const float2 sz = 0.5f * size;
    const float  w  = max(sz.x / values, 5.f);
    const uint   fg = getFGColor();
    const uint   bg = getBGColor();

    if (isDiscrete())
    {
        DMesh::Handle h(theDMesh());
        if (isBinary())
        {
            const uint bgc = true ? bg : 0x0;
            const uint fgc = hovered ? hoveredLineColor : defaultLineColor;
            PushButton(&h.mp.tri, &h.mp.line, position, sz, bgc, fgc, alpha);
            if (value) {
                h.mp.tri.translateZ(0.1f);
                PushButton(&h.mp.tri, &h.mp.line, position, max(float2(2.f), sz - kButtonPad), fgc, fgc, alpha);
                h.mp.tri.translateZ(-0.1f);
            }
        }
        else
        {
            float2 pos = position - justX(sz.x - w);
            const float2 bs = float2(w, sz.y) - kButtonPad;
            for (int i=0; i<values; i++)
            {
                const uint bgc = (i == value) ? bg : 0x0;
                const uint fgc = (i == hoveredValue) ? hoveredLineColor : defaultLineColor;
                PushButton(&h.mp.tri, &h.mp.line, pos, bs, bgc, fgc, alpha);
                if (i == value)
                {
                    h.mp.tri.translateZ(0.1f);
                    PushButton(&h.mp.tri, &h.mp.line, pos, max(float2(2.f), bs - kButtonPad), fgc, fgc, alpha);
                    h.mp.tri.translateZ(-0.1f);
                }
                pos.x += 2.f * w;
            }
        }
        h.Draw(s_);
    }
    else
    {
        ShaderState ss = s_;
        ss.color32(fg, alpha);
        ss.translateZ(0.1f);
        ShaderUColor::instance().DrawLine(ss, position - justX(sz), position + justX(sz));
        const float of = (size.x - 2.f * w) * (getValueFloat() - 0.5f);
        ss.translateZ(1.f);
        DrawButton(&ss, position + float2(of, 0.f), float2(w, sz.y), bg, fg, alpha);
    }
}

void OptionEditor::setValueFloat(float v)
{
    if (type == FLOAT)
        *(float*) value = v;
    else
        *(int*) value = round_int(v);
    txt = str_format("%s: %s", label, getTxt().c_str());
}

void OptionEditor::init(Type t, void *v, const char* lbl, const vector<const char*> &tt, float st, float mu, int states)
{
    type = t;
    value = v;
    start = st;
    mult = mu;
    slider.values = states;
    label = lbl;
    tooltip = tt;
    updateSlider();
}

string OptionEditor::getTxt() const
{
    if (slider.values <= 4) {
        const int val = getValueInt();
        if (slider.values == tooltip.size()) {
            return tooltip[clamp(val, 0, tooltip.size()-1)];
        } else if (slider.values == 3) {
            return (val == 0 ? "Off" : 
                    val == 1 ? "Low" :  "Full");
        } else if (slider.values == 4) {
            return (val == 0 ? "Off" : 
                    val == 1 ? "Low" : 
                    val == 2 ? "Medium" : "Full");
        } else {
            return val ? "On" : "Off";
        }
    } else if (start != 0.f) {
        return str_format("%d", getValueInt());
    } else {
        float val = 100.f * getValueFloat();
        return (val < 1.f) ? "Off" : str_format("%.0f%%", val);
    }
}

float2 OptionEditor::render(const ShaderState &ss, float alpha)
{
    slider.alpha = alpha;
    slider.render(ss);
    return GLText::Put(ss, slider.position + justX(0.5f * slider.size.x + 2.f * kButtonPad.x),
                       GLText::MID_LEFT, SetAlphaAXXX(kGUIText, alpha), 14, txt);
}

bool OptionEditor::HandleEvent(const Event* event, bool* valueChanged)
{
    bool changed = false;
    bool handled = slider.HandleEvent(event, &changed);
    if (handled && changed) {
        setValueFloat(slider.getValueFloat() * mult + start);
    }
    if (valueChanged)
        *valueChanged = changed;
    return handled;
}


void TabWindow::TabButton::renderButton(DMesh &mesh, bool)
{
    static const float o = 0.05f;
    const float2 r = size / 2.f;
    //   1      2
    // 0        3  
    const float2 v[] = {
        position + float2(-r),
        position + float2(lerp(-r.x, r.x, o), r.y),
        position + float2(r),
        position + float2(r.x, -r.y), 
    };
    mesh.tri.PushPoly(v, arraySize(v));
    mesh.line.PushStrip(v, arraySize(v));
}

float TabWindow::getTabHeight() const
{
    return 2.f * kPadDist + 1.5f * GLText::getScaledSize(textSize); 
}

void TabWindow::render(const ShaderState &ss)
{
    if (alpha > epsilon)
    {
        DMesh::Handle h(theDMesh());

        const float2 opos = position - float2(0.f, 0.5f * getTabHeight());
        const float2 osz = 0.5f * (size - float2(0.f, getTabHeight()));
        h.mp.translateZ(-1.5f);
        h.mp.tri.color32(defaultBGColor, alpha);
        h.mp.tri.PushRect(opos, osz);
        h.mp.line.translateZ(0.1f);
        h.mp.line.color32(defaultLineColor, alpha);

        const float2 tsize = float2(size.x / buttons.size(), getTabHeight());
        float2 pos = opos + flipX(osz);
        int i=0;
        foreach (TabButton& but, buttons)
        {
            but.size = tsize;
            but.position = pos + 0.5f * tsize;
            pos.x += tsize.x;

            h.mp.line.color32(!but.active ? inactiveLineColor :
                              but.hovered ? hoveredLineColor : defaultLineColor, alpha);
            h.mp.tri.color32((selected == i) ? defaultBGColor : inactiveBGColor, alpha);
            but.renderButton(h.mp);
            
            i++;
        }
        
        // 4 5 0 1
        // 3     2
        const float2 vl[] = {
            buttons[selected].position + flipY(buttons[selected].size / 2.f),
            opos + osz,
            opos + flipY(osz),
            opos - osz, 
            opos + flipX(osz), 
            buttons[selected].position - buttons[selected].size / 2.f
        };
        h.mp.line.color32(defaultLineColor, alpha);
        h.mp.line.PushStrip(vl, arraySize(vl));

        h.Draw(ss);

        foreach (const TabButton& but, buttons)
        {
            GLText::Put(ss, but.position, GLText::MID_CENTERED, MultAlphaAXXX(textColor, alpha),
                        textSize, but.text);
        }
    }
    buttons[selected].interface->renderTab(getContentsCenter(), getContentsSize(), alpha, alpha2);
}

int TabWindow::addTab(string txt, int ident, ITabInterface *inf)
{
    const int idx = (int)buttons.size();
    buttons.push_back(TabButton());
    TabButton &bu = buttons.back();
    bu.interface = inf;
    bu.text = txt;
    if (idx < 9)
        bu.keys[0] = str_format("%d", idx+1)[0];
    bu.ident = ident;
    return idx;
}

bool TabWindow::HandleEvent(const Event* event)
{
    if ( buttons[selected].interface->HandleEvent(event))
        return true;
    
    int i=0;
    bool isActivate = false;
    foreach (TabButton& but, buttons)
    {
        if (but.HandleEvent(event, &isActivate))
        {
            if (isActivate && selected != i &&
                buttons[selected].interface->onSwapOut())
            {
                selected = i;
                buttons[selected].interface->onSwapIn();
                globals.sound->OnButtonHover();
            }
            return true;
        }
        i++;
    }
    
    const int dkey = KeyState::instance().getDownKey(event);
    const bool isLeft = dkey == (MOD_SHFT | '\t') || dkey == GamepadLeftShoulder;
    const bool isRight = dkey == '\t' || dkey == GamepadRightShoulder;

    if (isLeft || isRight)
    {
        const int next = modulo(selected + (isLeft ? -1 : 1), buttons.size());
        if (next != selected &&
            buttons[selected].interface->onSwapOut())
        {
            selected = next;
            buttons[selected].interface->onSwapIn();
        }
        globals.sound->OnButtonHover();
        return true;
    }

    return false;
}

MessageBoxWidget::MessageBoxWidget()
{
    title = _("Message");
    okbutton.text = _("OK");
    okbutton.setReturnKeys();
}

void MessageBoxWidget::updateFade()
{
    static const float kMessageBoxFadeTime = 0.15f;
    static const float kMessageBoxTextFadeTime = 0.25f;
    
    alpha = lerp(alpha, active ? 1.f : 0.f, globals.frameTime / kMessageBoxFadeTime);
    alpha2 = active ? lerp(alpha2, 1.f, globals.frameTime / kMessageBoxTextFadeTime) : alpha;
}

void MessageBoxWidget::render(const ShaderState &ss, const View& view)
{
    if (alpha < epsilon)
        return;
    
    fadeFullScreen(ss, view, COLOR_BLACK, alpha * 0.5f);

    const float titleSize = 36;
    const float2 boxPad = 3.f * kButtonPad;
    const GLText *msg = GLText::get(messageFont, GLText::getScaledSize(14), message);
    
    size = max(0.6f * view.sizePoints,
               msg->getSize() + 6.f * boxPad +
               float2(0.f, GLText::getScaledSize(titleSize) + okbutton.size.y));
    
    position = 0.5f * view.sizePoints;
    
    const float2 boxRad = size / 2.f;

    DrawFilledRect(ss, position, boxRad, kGUIBg, kGUIFg, alpha);
        
    float2 pos = position + justY(boxRad) - justY(boxPad);
        
    pos.y -= GLText::Put(ss, pos, GLText::DOWN_CENTERED, MultAlphaAXXX(kGUIText, alpha),
                         titleSize, title).y;

    pos.x = position.x - boxRad.x + boxPad.x;

    DrawFilledRect(ss, position, msg->getSize() / 2.f + boxPad, kGUIBg, kGUIFg, alpha2);
    
    {
        ShaderState s1 = ss;
        s1.color(kGUIText, alpha);
        msg->render(&s1, position - msg->getSize() / 2.f);
    }

    okbutton.position = position - justY(boxRad) + justY(boxPad) + 0.5f * okbutton.size.y;
    okbutton.alpha = alpha2;
    okbutton.render(ss, false);
}

bool MessageBoxWidget::HandleEvent(const Event* event)
{
    if (!active)
        return false;
    bool isActivate = false;
    // const bool handled = ButtonHandleEvent(okbutton, event, "\r\033 ", &isActivate, NULL);
    if (okbutton.HandleEvent(event, &isActivate) && isActivate)
    {
        active = false;
    }
    // always handle when active
    return true;
}

static void setupHsvRect(VertexPosColor* verts, float2 pos, float2 rad, float alpha,
                         const std::initializer_list<float3> c)
{
    // 0 1
    // 2 3
    verts[0].pos = float3(pos + flipX(rad), 0.f);
    verts[1].pos = float3(pos + rad, 0.f);
    verts[2].pos = float3(pos - rad, 0.f);
    verts[3].pos = float3(pos + flipY(rad), 0.f);

    for (int i=0; i<4; i++)
        verts[i].color = ALPHAF(alpha)|rgb2bgr(rgbf2rgb(c.begin()[i]));
}

void ColorPicker::render(const ShaderState &ss)
{
    DrawFilledRect(ss, position, size/2.f, kGUIBg, kGUIFg, alpha);

    hueSlider.size = float2(size.x - 2.f * kButtonPad.x, 0.15f * size.y);
    hueSlider.position.x = position.x - size.x / 2.f + hueSlider.size.x / 2.f + kButtonPad.x;
    hueSlider.position.y = position.y + size.y / 2.f - hueSlider.size.y / 2.f - kButtonPad.y;

    svRectSize.y = size.y - hueSlider.size.y - 3.f * kButtonPad.y;
    svRectSize.x = svRectSize.y;
    svRectPos = position - size/2.f + kButtonPad + svRectSize / 2.f;
    
    const float2 csize = size - float2(svRectSize.x, hueSlider.size.y) - 3.f * kButtonPad;
    const float2 ccPos = position + flipY(size / 2.f) + flipX(kButtonPad + csize / 2.f);

    DMesh::Handle h(theDMesh());
    LineMesh<VertexPosColor> &lmesh = h.mp.line;
    
    // draw color picker
    {
        // 0 1
        // 2 3
        VertexPosColor verts[4];
        const uint indices[] = { 0,1,3, 0,3,2 };
        ShaderState s1 = ss;
        s1.color32(kGUIFg, alpha);

        setupHsvRect(verts, hueSlider.position, hueSlider.size/2.f, alpha, {
                float3(0.f, 1.f, 1.f), float3(M_TAOf, 1.f, 1.f),
                    float3(0.f, 1.f, 1.f), float3(M_TAOf, 1.f, 1.f) });

        DrawElements(ShaderHsv::instance(), ss, GL_TRIANGLES, verts, indices, arraySize(indices));
        s1.color32(hueSlider.getFGColor(), alpha);
        lmesh.color32(hueSlider.getFGColor(), alpha);
        lmesh.PushRect(hueSlider.position, hueSlider.size/2.f);

        const float hn = hsvColor.x / 360.f;
        setupHsvRect(verts, svRectPos, svRectSize/2.f, alpha, {
                float3(hn, 0.f, 1.f), float3(hn, 1.f, 1.f),
                    float3(hn, 0.f, 0.f), float3(hn, 1.f, 0.f) });

        DrawElements(ShaderHsv::instance(), ss, GL_TRIANGLES, verts, indices, arraySize(indices));
        lmesh.color32((svHovered || svDragging) ? kGUIFgActive : kGUIFg, alpha);
        lmesh.PushRect(svRectPos, svRectSize/2.f);
    }

    // draw indicators
    lmesh.color(GetContrastWhiteBlack(getColor()), alpha);
    lmesh.PushCircle(svRectPos - svRectSize/2.f + float2(hsvColor.y, hsvColor.z) * svRectSize,
                     4.f, 6);
    lmesh.color(GetContrastWhiteBlack(hsvf2rgbf(float3(hsvColor.x, 1.f, 1.f))), alpha);
    lmesh.PushRect(float2(hueSlider.position.x - hueSlider.size.x / 2.f + hueSlider.size.x * (hsvColor.x / 360.f),
                          hueSlider.position.y),
                   float2(kPadDist, hueSlider.size.y / 2.f));
    lmesh.Draw(ss, ShaderColor::instance());

    // draw selected color box
    ShaderState s1 = ss;
    s1.translateZ(1.1f);
    DrawFilledRect(s1, ccPos, csize/2.f, ALPHA_OPAQUE|getColor(), kGUIFg, alpha);
}


bool ColorPicker::HandleEvent(const Event* event, bool *valueChanged)
{
    if (hueSlider.HandleEvent(event, valueChanged))
    {
        hsvColor.x = hueSlider.getValueFloat() * 360.f;
        return true;
    }

    bool handled = false;
    if (event->isMouse())
        svHovered = intersectPointRectangle(event->pos, svRectPos, svRectSize/2.f);
    if (svHovered)
    {
        if (event->type == Event::MOUSE_DRAGGED ||
            event->type == Event::MOUSE_DOWN)
            svDragging = true;
        handled = (event->type != Event::MOUSE_MOVED);
    }

    if (event->isMouse() && svDragging)
    {
        if (event->type == Event::MOUSE_UP) {
            svDragging = false;
        } else {
            float2 pos = clamp((event->pos - (svRectPos - svRectSize / 2.f)) / svRectSize,
                               float2(0.f), float2(1.f));
            hsvColor.y = pos.x;
            hsvColor.z = pos.y;
            if (valueChanged)
                *valueChanged = true;
        }
        return true;
    }

    return handled;
}


void TextBox::Draw(const ShaderState& ss1, float2 point, const string& text) const
{
    if ((fgColor&ALPHA_OPAQUE) == 0 || alpha < epsilon)
        return;

    ShaderState ss = ss1;    
    const GLText* st = GLText::get(font, GLText::getScaledSize(tSize), text);
    const float2 boxSz = max(5.f + 0.5f * st->getSize(), box);

    float2 center = point;
    
    if (view)
    {
        float2 corneroffset = rad + st->getSize();
        for (int i=0; i<2; i++) {
            if (point[i] + corneroffset[i] > view->sizePoints[i]) {
                if (point[i] - corneroffset[i] > 0.f)
                    corneroffset[i] = -corneroffset[i];
                else {
                    point[i] = view->sizePoints[i] - corneroffset[i];
                }
            }
        }
        center = round(point + 0.5f * corneroffset);
    }

    ss.translate(center);
    ss.color32(bgColor, alpha);
    ss.translateZ(1.f);
    ShaderUColor::instance().DrawRect(ss, boxSz);
    ss.color32(fgColor, alpha);
    ss.translateZ(0.1f);
    ShaderUColor::instance().DrawLineRect(ss, boxSz);
    ss.translate(round(-0.5f * st->getSize()));
    st->render(&ss);
}

bool OverlayMessage::isVisible() const
{
    return message.size() && (globals.renderTime < startTime + totalTime);
}


bool OverlayMessage::setMessage(const string& msg, uint clr)
{
    std::lock_guard<std::mutex> l(mutex);
    bool changed = (msg != message) || (globals.renderTime > startTime + totalTime);
    message = msg;
    startTime = globals.renderTime;
    if (clr)
        color = clr;
    return changed;
}

void OverlayMessage::setVisible(bool visible)
{
    if (visible)
        startTime = globals.renderTime;
    else
        startTime = 0.f;
}

void OverlayMessage::render(const ShaderState &ss)
{
    std::lock_guard<std::mutex> l(mutex);
    if (!isVisible())
        return;
    const float a = alpha * inv_lerp(startTime + totalTime, startTime, (float)globals.renderTime);
    size = GLText::Put(ss, position, align, SetAlphaAXXX(color, a), textSize, message);
}

bool HandleConfirmKey(const Event *event, int* slot, int selected, bool *sawUp,
                  int key0, int key1, bool *isConfirm)
{
    if (*slot >= 0 && selected != *slot) {
        *slot = -1;
        return false;
    }

    if (!(event->isKey() && (event->key == key0 || event->key == key1)))
        return false;

    if (*slot == -1 && event->isDown())
    {
        *sawUp = false;
        *slot = selected;
    }
    else if (*slot >= 0 && event->isUp())
    {
        *sawUp = true;
    }
    else if (*slot >= 0 && *sawUp && event->isDown())
    {
        *slot = -1;
        *sawUp = false;
        *isConfirm = true;
    }
    globals.sound->OnButtonPress();
    return true;
}

bool HandleEventSelected(int* selected, ButtonBase &current, int count, int rows, 
                         const Event* event, bool* isActivate)
{
    bool handled = false;
    if (event->type == Event::KEY_DOWN)
    {
        switch (event->key)
        {
        case 'w':
        case NSUpArrowFunctionKey:
        case GamepadDPadUp:
        case GamepadLeftUp:
        case GamepadRightUp:
            *selected = modulo(*selected-1, count);
            globals.sound->OnButtonHover();
            handled = true;
            break;

        case 's':
        case NSDownArrowFunctionKey:
        case GamepadDPadDown:
        case GamepadLeftDown:
        case GamepadRightDown:
            *selected = modulo(*selected+1, count);
            globals.sound->OnButtonHover();
            handled = true;
            break;

        case 'a':
        case NSLeftArrowFunctionKey:
        case GamepadDPadLeft:
        case GamepadLeftLeft:
        case GamepadRightLeft:
            if (rows <= 1)
                return false;
            *selected = modulo(*selected - ((count+rows-1) / rows), count);
            globals.sound->OnButtonHover();
            handled = true;
            break;

        case 'd':
        case NSRightArrowFunctionKey:
        case GamepadDPadRight:
        case GamepadLeftRight:
        case GamepadRightRight:
            if (rows <= 1)
                return false;
            *selected = modulo(*selected + ((count+rows-1) / rows), count);
            globals.sound->OnButtonHover();
            handled = true;
            break;

        case '\r':
        case GamepadA:
            if (current.active)
            {
                *isActivate     = true;
                current.pressed = true;
                globals.sound->OnButtonPress();
            }
            else
            {
                globals.sound->OnButtonHover();   
            }
            return false;
        }
    }

    if (event->type == Event::KEY_UP)
    {
        current.pressed = false;
    }

    if (event->isGamepad())
    {
        // FIXME not quite right
        KeyState::instance().cursorPosScreen = current.position;
    }
    
    return handled;
}

bool ButtonHandleEvent(ButtonBase &button, const Event* event, bool* isActivate, bool* isPress, int *selected)
{
    const bool wasHovered = button.hovered;

    bool handled = false;
    bool activate = false;
    bool press = false;
    
    handled = button.HandleEvent(event, &activate, &press);
    
    if ((isActivate && activate) || (isPress && press))
    {
        globals.sound->OnButtonPress();
    }
    else if (!wasHovered && button.hovered && button.active)
    {
        globals.sound->OnButtonHover();
        if (selected)
            *selected = button.index;
    }

    if (isActivate && activate)
        *isActivate = true;
    if (isPress && press)
        *isPress = true;

    return handled;
}


void ButtonText::renderText(const ShaderState &ss, float2 pos, float width,
                GLText::Align align, uint color, float fmin, float fmax,
                const string& text)
{
    if (fontSize <= 0.f)
        fontSize = fmax;
    float tw = GLText::Put(ss, pos, align, color, fontSize, text).x;
    float ts = clamp(fontSize * ((width - 2.f * kButtonPad.x) / tw), fmin, fmax);
    if (fabsf(fontSize - ts) >= 1.f)
        fontSize = ts;
}


void Scrollbar::render(DMesh &mesh) const
{
    mesh.line.color(hovered ? hoveredFGColor : defaultFGColor, alpha);
    mesh.translateZ(0.5f);
    mesh.line.translateZ(0.1f);

    const float2 pos = steps ? position + justY(size.y/2.f * (1.f - (sfirst + min(sfirst + lines, (float)steps)) / steps)) : position;
    const float2 rad = float2(size.x, max(kScrollbarWidth/2.f,
                                          steps ? (min(lines, steps) * size.y / steps) : size.y)) / 2.f;
    mesh.line.PushRect(pos, rad);
    mesh.tri.color(pressed ? pressedBGColor : defaultBGColor, alpha);
    mesh.tri.PushRect(pos, rad);
}

bool Scrollbar::HandleEvent(const Event *event)
{
    if (!event->isMouse() && event->type != Event::SCROLL_WHEEL)
        return false;
    hovered = intersectPointRectangle(event->pos, position, size/2.f);
    if (steps == 0) {
        first = 0;
        lines = 0;
        hovered = false;
        pressed = false;
        sfirst = 0.f;
        return false;
    }
    const int maxfirst = steps - min(lines, steps);
    const bool parentHovered = (!parent || parent->hovered || hovered);
    const int page = min(1, lines-1);

    if (parentHovered)
    {
        if (event->type == Event::SCROLL_WHEEL &&
            fabsf(event->vel.y) > epsilon)
        {
            first = clamp(first + ((event->vel.y > 0) ? -1 : 1), 0, maxfirst);
            sfirst = first;
            return true;
        }

        if (event->isKeyDown(NSPageUpFunctionKey)) {
            first = clamp(first - page, 0, maxfirst);
            sfirst = first;
        } else if (event->isKeyDown(NSPageDownFunctionKey)) {
            first = clamp(first + page, 0, maxfirst);
            sfirst = first;
        }
    }
        
    if (!event->isMouse())
        return false;

    if (pressed)
    {
        if (event->type == Event::MOUSE_DRAGGED) {
            sfirst = clamp(sfirst + steps * event->vel.y / size.y, 0.f, (float)maxfirst);
            first = floor_int(sfirst);
            return true;
        } else {
            pressed = false;
            return true;
        }
    }
    if (!(hovered && event->type == Event::MOUSE_DOWN))
        return false;
    const float2 pos = position + justY(size.y/2.f * (1.f - float(first + last()) / steps));
    const float2 rad = float2(size.x, steps ? (min(lines, steps) * size.y / steps) : size.y) / 2.f;
    if (intersectPointRectangle(event->pos, pos, rad)) {
        pressed = true;         // start dragging thumb
    } else {
        // clicking in bar off of thumb pages up or down
        first = clamp(first + ((event->pos.y > pos.y) ? -page : page), 0, maxfirst);
        sfirst = first;
    }
    return true;
}

void ButtonWindow::render(const ShaderState &ss)
{
    DMesh::Handle h(theDMesh());
    h.mp.translateZ(-1.f);
    h.mp.line.color(hovered ? kGUIFgActive : kGUIFg, alpha / 2.f);
    h.mp.line.PushRect(position, size/2.f);
    h.mp.tri.color(kGUIBg, alpha / 2.f);
    h.mp.tri.PushRect(position, size/2.f);
    h.mp.translateZ(1.f);

    std::lock_guard<std::mutex> l(mutex);

    scrollbar.steps = (buttons.size() + (dims.x - 1)) / dims.x;
    scrollbar.lines = dims.y;
    if (scrollbar.first >= scrollbar.steps)
        scrollbar.first = 0;
    
    const int    first = scrollbar.first * dims.x;
    const int    last  = min(scrollbar.last() * dims.x, (int)buttons.size());
    const bool   sbvis = (first != 0 || last != buttons.size());
    const float  sw    = sbvis ? kScrollbarWidth : 0.f;
    const float2 bsize = size - kButtonPad;

    if (buttons.size())
    {
        const float2 bs = float2((bsize.x - sw), bsize.y) / float2(dims);
        float2 pos = position - flipY(bsize/2.f) + justX(bs.x / 2.f);
        const float posx = pos.x;
        for (int i=scrollbar.first; i<scrollbar.last(); i++)
        {
            pos.x = posx;
            pos.y -= bs.y/2.f;
            for (int j=0; j<dims.x; j++)
            {
                const int idx = i * dims.x + j;
                if (idx >= buttons.size())
                    break;
                ButtonBase *bu = buttons[idx];
                if (!dragPtr || bu != *dragPtr)
                    bu->position = pos;
                else
                    dragPos = pos;
                bu->size = bs - 2.f * kButtonPad;
                bu->alpha = alpha;
                bu->renderButton(h.mp);
                pos.x += bs.x;
            }
            pos.y -= bs.y/2.f;
        }
    }
    
    h.Draw(ss);
    h.mp.clear();

    for (int i=0; i<buttons.size(); i++)
    {
        buttons[i]->visible = (first <= i && i < last);
        if (buttons[i]->visible)
            buttons[i]->renderContents(ss);
    }

    if (sbvis)
    {
        // scrollbar
        scrollbar.alpha = alpha;
        scrollbar.position = position + justX(size/2.f - sw/2.f);
        scrollbar.size = float2(sw, size.y) - 2.f * kButtonPad;
        scrollbar.render(h.mp);
        h.Draw(ss);
    }
}

bool ButtonWindow::HandleEvent(const Event *event,
                               ButtonBase **activated,
                               ButtonBase **dragged,
                               ButtonBase **dropped)
{
    if (scrollbar.HandleEvent(event))
        return true;

    if (event->isMouse())
        hovered = intersectPointRectangle(event->pos, position, size/2.f);
    if (!hovered)
    {
        foreach (ButtonBase *bu, buttons)
            bu->hovered = false;
        return false;
    }

    bool handled = false;
    foreach (ButtonBase *&bu, buttons)
    {
        bool isActivate = false;
        bool isPress = false;
        handled |= ButtonHandleEvent(*bu, event,
                                     activated ? &isActivate : NULL,
                                     dragged ? &isPress : NULL);
        if (isActivate) {
            *activated = bu;
            handled = true;
        }
        if (dragged && bu->pressed && event->type == Event::MOUSE_DRAGGED) {
            *dragged = bu;
            handled = true;
        }
        if (dropped && bu->hovered && event->type == Event::MOUSE_UP) {
            *dropped = bu;
            handled = true;
        }
    }
    
    return handled || (dropped && event->type == Event::MOUSE_UP);
}

ButtonBase *ButtonWindow::HandleRearrange(const Event *event, ButtonBase *drag)
{
    std::lock_guard<std::mutex> l(mutex);
    if (drag && !dragPtr)
    {
        dragPos = drag->position;
        dragOffset = drag->position - event->pos;
    }
    ButtonBase **ptr = NULL;
    foreach (ButtonBase *&bu, buttons) {
        if (bu == drag) {
            ptr = &bu;
            break;
        }
    }
    dragPtr = ptr;
    if (!drag || !dragPtr)
        return NULL;
    const float2 rad = size/2.f - 2.f * kButtonPad;
    drag->position = clamp(event->pos + dragOffset,
                           position - rad + drag->size/2.f,
                           position + rad - drag->size/2.f);
    foreach (ButtonBase *&bu, buttons)
    {
        if (bu != *dragPtr &&
            distanceSqr(drag->position, bu->position) < distanceSqr(drag->position, dragPos))
        {
            std::swap(bu->position, dragPos);
            std::swap(bu, *dragPtr);
            return bu;
        }
    }
    return NULL;
}


void ButtonWindow::computeDims(int2 mn, int2 mx)
{
    if (buttons.empty())
        return;
    
    float2 bsize;
    const float count = min(mx.x * mx.y, (int)buttons.size());
    int2 ds;
    do {
        ds.x++;
        ds.y = ceil_int(count / ds.x);
        bsize = size / float2(ds);
        ds = clamp(ds, mn, mx);
    } while ((bsize.x > 2.f * bsize.y || ds.x * ds.y < count) &&
             ds.x <= mx.x && ds.y >= mn.y);

    dims = ds;
}

