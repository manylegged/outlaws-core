
//
// GUI.cpp - widget library
//

// Copyright (c) 2013-2014 Arthur Danskin
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


bool ButtonBase::HandleEvent(const Event* event, bool* isActivate, bool* isPress)
{
    const float2 sz = 0.5f*size;

    bool handled = false;
    if (event->type == Event::KEY_DOWN || event->type == Event::KEY_UP)
    {
        if (active && isEventKey(event, keys))
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
        hovered = intersectPointRectangle(event->pos, position, sz);

        handled = visible && hovered &&
                  ((event->type == Event::MOUSE_DOWN) ||
                   (event->type == Event::MOUSE_UP) ||
                   (event->type == Event::MOUSE_DRAGGED));
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
        else 
        {
            pressed = false;
        }
    }
    return handled;
}

bool ToggleButton::HandleEvent(const Event* event)
{
    bool isActivate = false;
    bool handled = ButtonBase::HandleEvent(event, &isActivate);
    if (handled && isActivate)
    {
        enabled = !enabled;
    }
    return handled;
}

bool ButtonBase::renderTooltip(const ShaderState &ss, const View& view, uint color, bool force) const
{
    if (tooltip.empty() || (!force && !hovered) || alpha < epsilon)
        return false;

    DrawTextBox(ss, view, position, size / 2.f, tooltip, 13,
                MultAlphaAXXX(color, alpha),
                ALPHAF(0.75f * alpha)|COLOR_BLACK, kMonoFont);
    return true;
}

void ButtonBase::renderSelected(const ShaderState &ss, uint bgcolor, uint linecolor, float alpha) const
{
    ShaderState s = ss;
    const float2 sz = 0.5f * size;
    const float2 p = position + float2(-sz.x - sz.y, 0);
    s.color32(bgcolor, alpha);
    ShaderUColor::instance().DrawTri(s, p + float2(0, sz.y), p + float2(sz.y / 2, 0), p + float2(0, -sz.y));
    s.color32(linecolor, alpha);
    ShaderUColor::instance().DrawLineTri(s, p + float2(0, sz.y), p + float2(sz.y / 2, 0), p + float2(0, -sz.y));
}

void Button::render(const ShaderState &s_, bool selected)
{
    if (!visible)
        return;
    ShaderState ss = s_;
    const GLText* tx = GLText::get(textFont, GLText::getScaledSize(textSize), text);
    if (text.size() && !(style&S_FIXED)) {
        size.y = tx->getSize().y + padding.y;
        size.x = max(max(size.x, tx->getSize().x + padding.x),
                     size.y * (float)kGoldenRatio);
    }

    if (style&S_BOX) {
        DrawFilledRect(ss, position, 0.5f * size, getBGColor(), getFGColor(selected), alpha);
    } else if (style&S_CORNERS) {
        DrawButton(&ss, position, 0.5f * size, getBGColor(), getFGColor(selected), alpha);
    }

    if (tx) {
        const uint tcolor = (!active) ? inactiveTextColor : textColor;
        ss.color32(tcolor, alpha);
        tx->render(&ss, position -0.5f * tx->getSize());
    }

    // draw selection triangle next to selected button
    if (selected)
    {
        renderSelected(ss, defaultBGColor, hoveredLineColor, alpha);
    }
}

void Button::renderButton(DMesh& mesh)
{
    if (!visible)
        return;

    if (text.size() && !(style&S_FIXED))
    {
        const GLText* tx = GLText::get(textFont, GLText::getScaledSize(textSize), text);
        size.x = max(size.x, tx->getSize().x + padding.x);
        size.y = tx->getSize().y + padding.y;
    }
    
    if (style&S_BOX) {
        PushRect(&mesh.tri, &mesh.line, position, 0.5f * size, getBGColor(), getFGColor(false), alpha);
    } else if (style&S_CORNERS) {
        PushButton(&mesh.tri, &mesh.line,  position, 0.5f * size, getBGColor(), getFGColor(false), alpha);
    }
}

void Button::renderText(const ShaderState &s_) const
{
    if (!visible)
        return;
    const uint tcolor = MultAlphaAXXX((!active) ? inactiveTextColor : textColor, alpha);
    GLText::DrawStr(s_, position, GLText::MID_CENTERED, tcolor, textSize, text);
}

void TextInputBase::setText(const char* text, bool setSize)
{
    lines.clear();
    lines.push_back(string());
    string* line = &lines.back();
    uint longestLine = 0;
    for (const char* ptr=text; *ptr != '\0'; ptr++) {
        if (*ptr == '\n') {
            lines.push_back(string());
            line = &lines.back();
        }
        else {
            line->push_back(*ptr);
            longestLine = max(longestLine, (uint)line->size());
        }
    }
    cursor = int2(lines[lines.size()-1].size(), lines.size()-1);

    if (setSize)
    {
        sizeChars = int2(longestLine + 1, lines.size());
    }
}

static bool forward_char(int2& cursor, deque<string> &lines, int offset)
{
    if (offset == -1)
    {
        if (cursor.x == 0) {
            if (cursor.y <= 0)
                return false;

            cursor.y--;
            cursor.x = lines[cursor.y].size();
        } else
            cursor.x--;
    }
    else if (offset == 1)
    {
        if (cursor.x == lines[cursor.y].size()) {
            if (cursor.y >= lines.size()-1)
                return false;
            cursor.y++;
            cursor.x = 0;
        } else
            cursor.x++; 
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

static char delete_char(int2& cursor, deque<string> &lines, int offset)
{
    if (0 > cursor.y || cursor.y >= lines.size()) {
        return '\0';
    } else if (cursor.x > 0) {
        cursor.x--;
        const char ret = lines[cursor.y][cursor.x];
        lines[cursor.y].erase(cursor.x, 1);
        return ret;
    } else if (cursor.y > 0) {
        int nx = lines[cursor.y-1].size();
        lines[cursor.y - 1].append(lines[cursor.y]);
        lines.erase(lines.begin() + cursor.y);
        cursor.y--;
        cursor.x = nx;
        return '\n';
    } else {
        return '\0';
    }
}

static void delete_region(int2& cursor, deque<string> &lines, int2 mark)
{
    while (mark != cursor && delete_char(mark, lines, -1));
}

bool TextInputBase::HandleEvent(const Event* event, bool *textChanged)
{
    if (locked)
        return false;

    active = forceActive || 
             (!locked && intersectPointRectangle(KeyState::instance().cursorPosScreen, 
                                                 position, 0.5f * size));
    hovered = active;

    if (textChanged)
        *textChanged = false;

    if (active && event->type == Event::KEY_DOWN)
    {            
        if (KeyState::instance()[OControlKey]) {
            switch (event->key)
            {
            case 'b': forward_char(cursor, lines, -1); break;
            case 'f': forward_char(cursor, lines, +1); break;
            case 'p': cursor.y--; break;
            case 'n': cursor.y++; break;
            case 'a': cursor.x = 0; break;
            case 'e': cursor.x = lines[cursor.y].size(); break;
            case 'k':
                if (cursor.x == lines[cursor.y].size() && cursor.y < lines.size()-1) {
                    lines[cursor.y].append(lines[cursor.y+1]);
                    lines.erase(lines.begin() + cursor.y + 1);
                } else
                    lines[cursor.y].erase(cursor.x); 
                break;
            case 'd': lines[cursor.y].erase(cursor.x, 1); break;
            case 'v':
            case 'y':
                insertText(OL_ReadClipboard());
                if (textChanged)
                    *textChanged = true;
                break;
            }
        } else if (KeyState::instance()[OAltKey]) {
            switch (event->key)
            {
            case NSRightArrowFunctionKey:
            case 'f': forward_when(cursor, lines, +1, isalnum); break;
            case NSLeftArrowFunctionKey:
            case 'b': forward_when(cursor, lines, -1, isalnum); break;
            case NSDeleteCharacter:
            case NSBackspaceCharacter: {
                forward_when(cursor, lines, -1, isalnum);
                const int2 scursor = cursor;
                delete_region(cursor, lines, scursor);
                if (textChanged)
                    *textChanged = true;
                break;
            }
            case 'd': {
                const int2 scursor = cursor;
                forward_when(cursor, lines, 1, isalnum);
                delete_region(cursor, lines, scursor);
                if (textChanged)
                    *textChanged = true;
                break;
            }
            case 'm':
                cursor.x = 0;
                forward_when(cursor, lines, +1, isspace);
                break;
            }
        } else if (event->key < 256 && std::isprint(event->key)) {
            lines[cursor.y].insert(cursor.x, 1, event->key);
            cursor.x++;
            if (textChanged)
                *textChanged = true;
        } else {
            switch (event->key)
            {
            case NSLeftArrowFunctionKey:  forward_char(cursor, lines, -1); break;
            case NSRightArrowFunctionKey: forward_char(cursor, lines, +1); break;
            case NSUpArrowFunctionKey:    cursor.y = max(0, cursor.y-1); break;
            case NSDownArrowFunctionKey:  cursor.y = min((int)lines.size()-1, cursor.y+1); break;
            case NSDeleteCharacter:
            case NSBackspaceCharacter:
                delete_char(cursor, lines, -1);
                if (textChanged)
                    *textChanged = true;
                break;
            case '\r':
            {
                string s = lines[cursor.y].substr(cursor.x);
                lines[cursor.y].erase(cursor.x);
                lines.insert(lines.begin() + cursor.y + 1, s);
                if (textChanged)
                    *textChanged = true;
                cursor.y++;
                cursor.x = 0;
                break;
            }
            case NSPageUpFunctionKey: 
                startChars.y -= sizeChars.y;
                return true;
            case NSPageDownFunctionKey: 
                startChars.y += sizeChars.y;
                return true;
            default:
                return false;
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
    else if (active && event->type == Event::SCROLL_WHEEL)
    {
        startChars.y += ceil(-event->vel.y);
        return true;
    }

    return false;
}

void TextInputBase::pushText(const char *txt)
{
    vector<string> nlines = str_split(txt, '\n');
    lines.insert(lines.end()-1, nlines.begin(), nlines.end());

    cursor.y = (int)lines.size() - 1;
    cursor.x = lines[cursor.y].size();
    scrollForInput();
}

void TextInputBase::insertText(const char *txt)
{
    vector<string> nlines = str_split(txt, '\n');
    if (nlines.size() == 0)
        return;

    lines[cursor.y].insert(cursor.x, nlines[0]);
    cursor.x += nlines[0].size();

    lines.insert(lines.begin() + cursor.y + 1, nlines.begin() + 1, nlines.end());
    cursor.y += nlines.size()-1;
    
    scrollForInput();
}


void TextInputBase::render(const ShaderState &s_)
{
    ShaderState s = s_;

    startChars.x = 0;
    startChars.y = clamp(startChars.y, 0, max(0, (int)lines.size() - sizeChars.y));

    const int drawLines = min((int)lines.size() - startChars.y, sizeChars.y);

    float2 longestPointSize(0);
    int    longestChars     = 0;
    for (int i=startChars.y; i<(startChars.y + drawLines); i++)
    {
        const GLText* tx = GLText::get(kMonoFont, GLText::getScaledSize(textSize), lines[i].c_str());
        if (tx->getSize().x > longestPointSize.x) {
            longestPointSize = tx->getSize();
            longestChars     = lines[i].size();
        }
    }
    const float charHeight = GLText::getFontHeight(kMonoFont, GLText::getScaledSize(textSize));
    sizeChars.x = max(sizeChars.x, (int)longestChars + 1);
        
    //sizePoints.x = max(sizePoints.x, 2.f * kPadDist + longestPointSize.x);
    size.y = charHeight * sizeChars.y + kPadDist;

    const float2 sz = 0.5f * size;

    s.translate(position);

    const uint bgColor = MultAlphaAXXX(active ? activeBGColor : defaultBGColor, alpha);
    ShaderUColor::instance().DrawColorRect(s, bgColor, sz);

    const uint lineColor = MultAlphaAXXX(active ? activeLineColor : defaultLineColor, alpha);
    ShaderUColor::instance().DrawColorLineRect(s, lineColor, sz);

    s.translate(float2(-sz.x + kPadDist, sz.y - charHeight - kPadDist));
    s.color32(textColor, alpha);
        
    for (int i=startChars.y; i<(startChars.y + drawLines); i++)
    {
//            const string txt = lines[i].substr(startChars.x,
//                                               min(lines[i].size(), startChars.x + sizeChars.x));
        const GLText* tx = GLText::get(kMonoFont, GLText::getScaledSize(textSize), lines[i].c_str());

        if (lines[i].size())
        {
            updateState(i, &s);
            tx->render(&s);
        }
        
        // draw cursor
        if (active && cursor.y == i) {
            ShaderState  s1    = s;
            const float  start = tx->getCharStart(cursor.x);
            const float2 size  = tx->getCharSize(cursor.x);
            s1.translate(float2(start, 0));
            s1.color(textColor, alpha);
            ShaderUColor::instance().DrawRectCorners(s1, float2(0), size);
            if (cursor.x < lines[i].size()) {
                GLText::DrawStr(s1, float2(0.f), GLText::LEFT, kMonoFont,
                                ALPHA_OPAQUE|GetContrastWhiteBlack(textColor),
                                textSize, lines[i].substr(cursor.x, 1));
            }
        }
        
        s.translate(float2(0.f, -charHeight));
    }
}

void TextInputCommandLine::saveHistory(const char *fname)
{
    string str;
    foreach (string& line, commandHistory) {
        str += line;
        str += "\n";
    }
    OL_SaveFile(fname, str.c_str());
}

void TextInputCommandLine::loadHistory(const char *fname)
{
    const char* data = OL_LoadFile(fname);
    if (data) {
        str_split(data, '\n', commandHistory);
    }
    historyIndex = commandHistory.size();
}

string TextInputCommandLine::helpCmd(void* data, const char* name, const char* args) 
{
    TextInputCommandLine* self = (TextInputCommandLine*) data;
        
    const string arg = str_strip(args);
    string s;
    if (map_contains(self->commands, arg))
    {
        std::map<string, Command>::iterator it=self->commands.find(arg);
        s += it->first;
        s += " ";
        s += it->second.description;
        s += "\n";
    }
    else
    {
        for (std::map<string, Command>::iterator it=self->commands.begin(), end=self->commands.end(); it != end; ++it)
        {
            s += it->first;
            s += " ";
            s += it->second.description;
            s += "\n";
        }
    }
    return s;
}


void TextInputCommandLine::pushCmdOutput(const char *format, ...)
{
    va_list vl;
    va_start(vl, format);
    string txt = str_vformat(format, vl);
    va_end(vl);        

    vector<string> nlines = str_split(txt, '\n');
    lines.insert(lines.end(), nlines.begin(), nlines.end());
    lines.push_back(prompt);

    cursor.y = (int)lines.size() - 1;
    cursor.x = lines[cursor.y].size();
    scrollForInput();
}

bool TextInputCommandLine::doCommand(const string& line)
{
    pushHistory(line);
    const vector<string> expressions = str_split(line, ';');
    foreach (const string &expr, expressions)
    {
        vector<string> args = str_split(str_strip(expr), ' ');
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
            vector<string> possible;
            foreach (const auto& x, commands) {
                if (x.first.size() > cmd.size() && x.first.substr(0, cmd.size()) == cmd)
                    possible.push_back(x.first);
            }
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
        
        const vector<string> nlines = str_split(ot, '\n');
        lines.insert(lines.end(), nlines.begin(), nlines.end());
    }
    pushCmdOutput("");      // prompt
    return true;
}

const TextInputCommandLine::Command *TextInputCommandLine::getCommand(const string &abbrev) const
{
    const string cmd = str_tolower(abbrev);
    if (map_contains(commands, cmd)) {
        return map_get_addr(commands, cmd);
    } else {
        vector<string> possible;
        foreach (const auto& x, commands) {
            if (x.first.size() > cmd.size() && x.first.substr(0, cmd.size()) == cmd)
                possible.push_back(x.first);
        }
        if (possible.size() == 1) {
            return map_get_addr(commands, possible[0]);
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
        switch (event->key)
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
        case 'r':
        case 's':
        case 'p':
        case 'n':
        {
            int end = 0;
            int delta = 0;

            if ((KeyState::instance()[OAltKey] && event->key == 'p') ||
                (KeyState::instance()[OControlKey] && event->key == 'r'))
            {
                end = 0;
                delta = -1;
            }
            else if ((KeyState::instance()[OAltKey] && event->key == 'n') ||
                     (KeyState::instance()[OControlKey] && event->key == 's'))
            {
                end = commandHistory.size();
                delta = 1;
            }

            if (delta != 0)
            {
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
            break;
        }
        case '\t':
        {
            lastSearch = "";
            string line = getLineText();
            int start = line.rfind(' ');
            vector<string> options;
            if (start > 0)
            {
                vector<string> args = str_split(str_strip(line), ' ');
                if (args.size() >= 1)
                {
                    const Command *cmd = getCommand(args[0]);
                    if (cmd && cmd->comp)
                    {
                        const string argstr = str_join(" ", args.begin() + 1, args.end());
                        const string largs = str_tolower(argstr);
                        options = cmd->comp(cmd->data, cmd->name.c_str(), argstr.c_str());
                        for (int i=0; i<options.size(); ) {
                            vector_remove_increment(
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
                    string opts = str_word_wrap(str_join(" ", options.begin(), options.end()),
                                                sizeChars.x);
                    pushText((prompt + oline).c_str());;
                    pushText(opts.c_str());
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

    if (lines[cursor.y].size() < prompt.size()) {
        setLineText("");
    } else if (lines[cursor.y].substr(0, prompt.size()) != prompt) {
        for (int i=0; i<prompt.size(); i++) {
            lines[cursor.y][i] = prompt[i];
        }
    }

    if (handled && textChanged)
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
        
    GLText::DrawStr(s, position, GLText::LEFT, titleTextColor, textSize, title);

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
        GLText::DrawStr(s, position, GLText::LEFT, color, textSize, lines[i]);
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
    const float2 sz = 0.5f*size;
    hovered = pressed || intersectPointRectangle(event->pos, position, sz);
    pressed = (hovered && event->type == Event::MOUSE_DOWN) ||
              (pressed && event->type == Event::MOUSE_DRAGGED);

    bool handled = pressed || (hovered && event->isMouse());

    if (pressed) {
        const int lastv = value;
        const float v = ((event->pos.x - position.x) / size.x) + 0.5f;
        setValueFloat(v);
        if (valueChanged)
            *valueChanged = (value != lastv);
    }

    return handled;
}

void OptionSlider::render(const ShaderState &s_)
{
    ShaderState ss = s_;
    const float2 sz = 0.5f * size;

    ss.color(getFGColor(), alpha);
    ShaderUColor::instance().DrawLine(ss, position - float2(sz.x, 0.f), 
                                      position + float2(sz.x, 0.f));
    const float w = max(sz.x / values, 5.f);
    DrawButton(&ss, position + float2((size.x - 2.f * w) * (getValueFloat() - 0.5f), 0.f),
               float2(w, sz.y), getBGColor(), getFGColor(), alpha);
}


void TabWindow::render(const ShaderState &ss)
{
    DMesh mesh;

    const float2 opos = position - float2(0.f, 0.5f * getTabHeight());
    const float2 osz = 0.5f * (size - float2(0.f, getTabHeight()));
    mesh.tri.color32(defaultBGColor, alpha);
    mesh.tri.PushRect(opos, osz);
    mesh.line.color32(defaultLineColor, alpha);

    const float2 tsize = float2(size.x / buttons.size(), getTabHeight());
    float2 pos = opos + flipX(osz);
    int i=0;
    foreach (TabButton& but, buttons)
    {
        but.size = tsize;
        but.position = pos + 0.5f * tsize;
        pos.x += tsize.x;

        mesh.line.color32(!but.active ? inactiveLineColor :
                        but.hovered ? hoveredLineColor : defaultLineColor, alpha);
        mesh.tri.color32((selected == i) ? defaultBGColor : inactiveBGColor, alpha);
        but.render(mesh);
            
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
    mesh.line.color32(defaultLineColor, alpha);
    mesh.line.PushStrip(vl, arraySize(vl));

    mesh.Draw(ss, ShaderColor::instance(), ShaderColor::instance());

    foreach (const TabButton& but, buttons)
    {
        GLText::DrawStr(ss, but.position, GLText::MID_CENTERED, MultAlphaAXXX(textColor, alpha),
                        textSize, but.text);
    }

    buttons[selected].interface->renderTab(getContentsCenter(), getContentsSize(), alpha, alpha2);
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
            if (isActivate) {
                if (i != selected)
                    buttons[selected].interface->onSwapOut();
                selected = i;
            }
            return true;
        }
        i++;
    }
    if (event->type == Event::KEY_DOWN && event->key == '\t')
    {
        int next;
        if (KeyState::instance()[OShiftKey])
            next = modulo(selected-1, buttons.size());
        else
            next = (selected+1) % buttons.size();
        if (next != selected)
            buttons[selected].interface->onSwapOut();
        selected = next;
        return true;
    }

    return false;
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
    const GLText *msg = GLText::get(messageFont, GLText::getScaledSize(16), message);
    
    size = max(0.6f * view.sizePoints,
               msg->getSize() + 6.f * boxPad +
               float2(0.f, GLText::getScaledSize(titleSize) + okbutton.size.y));
    
    position = 0.5f * view.sizePoints;
    
    const float2 boxRad = size / 2.f;

    DrawFilledRect(ss, position, boxRad, kGUIBg, kGUIFg, alpha);
        
    float2 pos = position + justY(boxRad) - justY(boxPad);
        
    pos.y -= GLText::DrawStr(ss, pos, GLText::DOWN_CENTERED, MultAlphaAXXX(kGUIText, alpha),
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
        verts[i].color = ALPHAF(alpha)|RGB2BGR(RGBf2RGB(c.begin()[i]));
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
    
    LineMesh<VertexPosColor> lmesh;
    
    // draw color picker
    {
        // 0 1
        // 2 3
        VertexPosColor verts[4];
        const uint indices[] = { 0,1,3, 0,3,2 };
        ShaderState s1 = ss;
        s1.color(kGUIFg, alpha);

        setupHsvRect(verts, hueSlider.position, hueSlider.size/2.f, alpha, {
                float3(0.f, 1.f, 1.f), float3(M_TAOf, 1.f, 1.f),
                float3(0.f, 1.f, 1.f), float3(M_TAOf, 1.f, 1.f) });

        DrawElements(ShaderHsv::instance(), ss, GL_TRIANGLES, verts, indices, arraySize(indices));
        s1.color(hueSlider.getFGColor(), alpha);
        lmesh.color(hueSlider.getFGColor(), alpha);
        lmesh.PushRect(hueSlider.position, hueSlider.size/2.f);

        const float hn = hsvColor.x / 360.f;
        setupHsvRect(verts, svRectPos, svRectSize/2.f, alpha, {
                float3(hn, 0.f, 1.f), float3(hn, 1.f, 1.f),
                float3(hn, 0.f, 0.f), float3(hn, 1.f, 0.f) });

        DrawElements(ShaderHsv::instance(), ss, GL_TRIANGLES, verts, indices, arraySize(indices));
        lmesh.color((svHovered || svDragging) ? kGUIFgActive : kGUIFg, alpha);
        lmesh.PushRect(svRectPos, svRectSize/2.f);
    }

    // draw indicators
    lmesh.color(GetContrastWhiteBlack(getColor()), alpha);
    lmesh.PushCircle(svRectPos - svRectSize/2.f + float2(hsvColor.y, hsvColor.z) * svRectSize,
                     4.f, 6);
    lmesh.color(GetContrastWhiteBlack(rgbOfHsv(float3(hsvColor.x, 1.f, 1.f))), alpha);
    lmesh.PushRect(float2(hueSlider.position.x - hueSlider.size.x / 2.f + hueSlider.size.x * (hsvColor.x / 360.f),
                          hueSlider.position.y),
                   float2(kPadDist, hueSlider.size.y / 2.f));
    lmesh.Draw(ss, ShaderColor::instance());

    // draw selected color box
    DrawFilledRect(ss, ccPos, csize/2.f, ALPHA_OPAQUE|getColor(), kGUIFg, alpha);
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


