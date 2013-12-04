
//
// GUI.h - widget library
//

// Copyright (c) 2013 Arthur Danskin
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


#ifndef GUI_H
#define GUI_H

#include "GLText.h"
#include "Colors.h"
#include "Shaders.h"
#include <cctype>

static const double kDoubleClickTimeout = 0.25f;
static const double kTripleClickTimeout = 0.3f;
static const float  kPadDist            = 2;

template <typename T, typename U>
float2 positionBelowLeftAligned(const T& a, const U& b, float pad)
{
    const float2 asz = 0.5f * a.getSizePoints();
    const float2 bsz = 0.5f * b.getSizePoints();
    float2 pos;
    pos.y = b.position.y - asz.y - bsz.y - pad;
    pos.x = b.position.x + asz.x - bsz.x;
    return pos;
}

struct WidgetBase {
    float2      position;       // center of button
    float2      size;           // width x height in points
    bool        hovered = false;
    bool        active  = true;
    float       alpha   = 1.f;

    float2 getSizePoints() const { return size; }

};

struct ButtonBase : public WidgetBase {
    
    const char* keys = NULL;

    bool        pressed = false;
    bool        visible = true;
    string      tooltip;

    bool HandleEvent(const Event* event, bool* isActivate, bool* isPress=NULL)
    {
        float2 sz = 0.5f*size;

        bool handled = false;
        if (event->type == Event::KEY_DOWN || event->type == Event::KEY_UP)
        {
            if (active && keys && (event->key < 255) && strchr(keys, event->key))
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

            bool wasPressed = pressed;
            if (active && isActivate && pressed && handled && (event->type == Event::MOUSE_UP)) {
                *isActivate = true;
                pressed = false;
            } else if (active && visible && hovered) {
                if (event->type == Event::MOUSE_DOWN)
                    pressed = true;
            } else {
                pressed = false;
            }
            if (active && isPress)
                *isPress = (pressed && !wasPressed);
        }
        return handled;
    }

    void renderTooltip(const ShaderState &ss, const View& view, uint color) const
    {
        if (tooltip.empty() || !hovered || alpha < epsilon)
            return;

        DrawTextBox(ss, view, position, size, tooltip, 12, 
                    MultAlphaARGB(color, alpha),
                    MultAlphaARGB(COLOR_BLACK, 0.5f * alpha));
    }
};

struct Button : public ButtonBase
{
    string text;
    int    textSize          = 24.f;
    uint   defaultBGColor    = ALPHA(0xa0)|COLOR_TEXT_BG;
    uint   pressedBGColor    = ALPHA(0xb0)|COLOR_BG_GRID;
    uint   defaultLineColor  = ALPHA_OPAQUE|COLOR_ENEMY;
    uint   hoveredLineColor  = ALPHA_OPAQUE|COLOR_ENEMY_HI; 
    uint   inactiveLineColor = COLOR_INACTIVE;
    uint   textColor         = ALPHA_OPAQUE|COLOR_UI_ACTION;
    uint   inactiveTextColor = COLOR_INACTIVE;

    Button() {}
    Button(const string& str) : text(str) {}

    void setColors(uint txt, uint defBg, uint pressBg, uint defLine, uint hovLine)
    {
        textColor        = txt;
        defaultBGColor   = defBg;
        pressedBGColor   = pressBg;
        defaultLineColor = defLine;
        hoveredLineColor = hovLine;
    }

    void   setText(const char * t) { text = t; }
    string getText() const         { return text; }

    uint getBGColor() const { return pressed ? pressedBGColor : defaultBGColor; }
    uint getFGColor(bool selected) const { return ((!active) ? inactiveLineColor :
                                                   (hovered||selected) ? hoveredLineColor : defaultLineColor); }
    
    void render(const ShaderState *s_, bool selected=false)
    {
        if (!visible)
            return;
        ShaderState s = *s_;
        const GLText* t = GLText::get(GLText::getScaledSize(textSize), text);
        size = max(size, t->getSize() + float2(2 * kPadDist));
        const float2 sz = 0.5f * size;

        DrawButton(&s, position, sz, getBGColor(), getFGColor(selected), alpha);

        const uint tcolor = (!active) ? inactiveTextColor : textColor;
        s.color32(tcolor, alpha);
        t->render(&s, position -0.5f * t->getSize());

        // draw selection triangle next to selected button
        if (selected)
        {
            float2 p = position + float2(-sz.x - sz.y, 0);
            s.color32(defaultBGColor, alpha);
            ShaderUColor::instance().DrawTri(s, p + float2(0, sz.y), p + float2(sz.y / 2, 0), p + float2(0, -sz.y));
            s.color32(hoveredLineColor, alpha);
            ShaderUColor::instance().DrawLineTri(s, p + float2(0, sz.y), p + float2(sz.y / 2, 0), p + float2(0, -sz.y));
        }
    }

    void renderText(const ShaderState &s_)
    {
        if (!visible)
            return;
        const uint tcolor = MultAlphaARGB((!active) ? inactiveTextColor : textColor, alpha);
        size = max(size, GLText::DrawStr(s_, position, GLText::MID_CENTERED, tcolor, textSize, text));
    }
};

inline bool ButtonHandleEvent(ButtonBase &button, const Event* event, const char* keys, bool* isActivate, bool* isPress=NULL)
{
    const bool wasHovered = button.hovered;

    bool handled = false;
    if (button.active &&
        (event->type == Event::KEY_DOWN || event->type == Event::KEY_UP) && 
        keys && (event->key < 255) && strchr(keys, event->key) &&
        !globals.keyState[OControlKey] && !globals.keyState[OShiftKey] &&
        !globals.keyState[OAltKey])
    {
        handled = true;
        bool activate =!button.pressed && (event->type == Event::KEY_DOWN);
        button.pressed = activate;
        if (isActivate)
            *isActivate = activate;
    }
    else
    {
        handled = button.HandleEvent(event, isActivate, isPress);
    }
                         
    if ((isActivate && *isActivate) || (isPress && *isPress))
    {
        globals.sound->OnButtonPress();
    } 
    else if (!wasHovered && button.hovered && button.active)
    {
        globals.sound->OnButtonHover();
    }

    return handled;
}

inline void ButtonSetPos(ButtonBase& button, float2 pos)
{
    if (isZero(button.position)) {
        button.position = pos;
    } else {
        button.position = lerp(button.position, pos,
                               0.5f * globals.getView().sizePixels.y * globals.frameTime);
    }
}


struct TextInputBase : public WidgetBase {
    float2        position;     // center
    float2        sizePoints;   // width x height
    deque<string> lines;
    int           textSize;

    int2          sizeChars;
    int2          startChars;
    int2          cursor;
    bool          active;       // is currently editable?
    bool          locked;       // set to true to disable editing
    bool          forceActive;  // set to true to enable editing regardless of mouse position

    uint          defaultBGColor;
    uint          activeBGColor;
    uint          defaultLineColor;
    uint          activeLineColor;
    uint          textColor;
    float         alpha;

    TextInputBase()
    {
        lines.push_back(string());
        textSize = 12;

        sizeChars  = int2(80, 2);
        startChars = int2(0, 0);
        cursor     = int2(0, 0);

        active  = false;
        locked  = false;
        forceActive = false;

        defaultBGColor   = ALPHA(0x90)|COLOR_TEXT_BG;
        activeBGColor    = ALPHA(0xa0)|COLOR_BG_GRID;
        defaultLineColor = ALPHA_OPAQUE|COLOR_ENEMY;
        activeLineColor  = ALPHA_OPAQUE|COLOR_ENEMY_HI;
        textColor        = ALPHA_OPAQUE|COLOR_UI_ACTION;
        alpha            = 1.f;
    }
    
    virtual ~TextInputBase() {}

    string getText() const
    {
        string s;
        for (uint i=0; i<lines.size(); i++) {
            s += lines[i];
            if (i != lines.size()-1)
                s += "\n";
        }
        return s;
    }

    void setText(const char* text, bool setSize=false)
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

    bool HandleEventUpdateValue(const Event* event, string *value)
    {
        bool textChanged = false;
        if (HandleEvent(event, &textChanged)) {
            if (textChanged) {
                *value = getText();
            }
            return true;
        }
        return false;
    }

    bool HandleEvent(const Event* event, bool *textChanged=NULL)
    {
        if (locked)
            return false;

        active = forceActive || 
                 (!locked && intersectPointRectangle(globals.cursorPosScreen, 
                                                     position, 0.5f * sizePoints));

        if (textChanged)
            *textChanged = false;

        if (active && event->type == Event::KEY_DOWN)
        {            
            if (globals.keyState[OControlKey]) {
                switch (event->key)
                {
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
                }
            } else if (std::isprint(event->key)) {
                lines[cursor.y].insert(cursor.x, 1, event->key);
                if (textChanged)
                    *textChanged = true;
                cursor.x++;
            } else 
                switch (event->key)
                {
                case NSLeftArrowFunctionKey:  
                    if (cursor.x == 0) {
                        cursor.y--;
                        cursor.x = lines[cursor.y].size();
                    } else
                        cursor.x--;
                    break;
                case NSRightArrowFunctionKey:
                    if (cursor.x == lines[cursor.y].size()) {
                        cursor.y++;
                        cursor.x = 0;
                    } else
                        cursor.x++; 
                    break;
                case NSUpArrowFunctionKey:    cursor.y--; break;
                case NSDownArrowFunctionKey:  cursor.y++; break;
                case NSDeleteCharacter:
                case NSBackspaceCharacter:
                    if (cursor.x > 0) {
                        cursor.x--;
                        lines[cursor.y].erase(cursor.x, 1);
                    } else if (cursor.y > 0) {
                        int nx = lines[cursor.y-1].size();
                        lines[cursor.y - 1].append(lines[cursor.y]);
                        lines.erase(lines.begin() + cursor.y);
                        cursor.y--;
                        cursor.x = nx;
                    }
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

            cursor.y = clamp(cursor.y, 0, (int)lines.size()-1);
            cursor.x = clamp(cursor.x, 0, lines[cursor.y].size());
            scrollForInput();
            
            return true;
        }
        else if (active && event->type == Event::SCROLL_WHEEL)
        {
            startChars.y += ceil(event->vel.y);
            return true;
        }

        return false;
    }

    void pushText(const char *format, ...)
    {
        va_list vl;
        va_start(vl, format);
        string txt = str_vformat(format, vl);
        va_end(vl);        

        vector<string> nlines = str_split(txt, '\n');
        lines.insert(lines.end()-1, nlines.begin(), nlines.end());

        cursor.y = (int)lines.size() - 1;
        cursor.x = lines[cursor.y].size();
        scrollForInput();
    }

    void scrollForInput()
    {
        startChars.y = clamp(startChars.y, max(0, cursor.y - sizeChars.y + 1), cursor.y);
    }

    bool isPress(const Event* event) const
    {
        bool isPress = (event->key == 0) && 
                       ((event->type == Event::MOUSE_DOWN) /*|| (event->type == Event::MOUSE_DRAGGED)*/) &&
                       intersectPointRectangle(event->pos, position, 0.5f*sizePoints);
        return isPress;
    }

    int2 getSizeChars() const    { return sizeChars; }
    float2 getSizePoints() const { return sizePoints; }
    bool blinkOn() const         { return fmodf(globals.renderTime, 0.5f) > 0.25f; }

    virtual void updateState(int line, ShaderState* ss){}

    void render(const ShaderState *s_)
    {
        ShaderState s = *s_;

        startChars.x = 0;
        startChars.y = clamp(startChars.y, 0, max(0, (int)lines.size() - sizeChars.y));

        const int drawLines = min((int)lines.size() - startChars.y, sizeChars.y);

        float2 longestPointSize(0);
        int    longestChars     = 0;
        for (int i=startChars.y; i<(startChars.y + drawLines); i++)
        {
            const GLText* t = GLText::get(GLText::getScaledSize(textSize), lines[i].c_str());
            if (t->getSize().x > longestPointSize.x) {
                longestPointSize = t->getSize();
                longestChars     = lines[i].size();
            }
        }
        const float charHeight = longestPointSize.y;
        sizeChars.x = max(sizeChars.x, (int)longestChars + 1);
        
        //sizePoints.x = max(sizePoints.x, 2.f * kPadDist + longestPointSize.x);
        sizePoints.y = charHeight * sizeChars.y;

        const float2 sz = 0.5f * sizePoints;

        s.translate(position);

        const uint bgColor = MultAlphaARGB(active ? activeBGColor : defaultBGColor, alpha);
        ShaderUColor::instance().DrawColorRect(s, bgColor, sz);

        const uint lineColor = MultAlphaARGB(active ? activeLineColor : defaultLineColor, alpha);
        ShaderUColor::instance().DrawColorLineRect(s, lineColor, sz);

        s.translate(float2(-sz.x + kPadDist, sz.y - charHeight - kPadDist));
        s.color32(textColor, alpha);
        
        for (int i=startChars.y; i<(startChars.y + drawLines); i++)
        {
//            const string txt = lines[i].substr(startChars.x,
//                                               min(lines[i].size(), startChars.x + sizeChars.x));
            const GLText* t = GLText::get(GLText::getScaledSize(textSize), lines[i].c_str());
            
            // draw cursor
            if (active && cursor.y == i && blinkOn()) {
                ShaderState  s1    = s;
                const float  start = t->getCharStart(cursor.x);
                const float2 size  = float2(t->getCharSize(cursor.x).x, charHeight);
                s1.translate(float2(start, 0));
                s1.color(textColor, alpha);
                ShaderUColor::instance().DrawRectCorners(s1, float2(0), size);
            }

            if (lines[i].size())
            {
                updateState(i, &s);
                t->render(&s);
            }
            s.translate(float2(0.f, -charHeight));
        }
    }

};

struct TextInputCommandLine : public TextInputBase {

    typedef string (*CommandFunc)(void* data, const char* name, const char* args);

    struct Command {
        CommandFunc func;
        void*       data;
        string      description;
    };

    vector<string>            commandHistory;
    int                       historyIndex;
    std::map<string, Command> commands;
    string                    prompt;

    TextInputCommandLine()
    {
        sizeChars.y = 10;
        prompt = "> ";

        registerCommand(helpCmd, this, "help", "[command]: list help for specifid command, or all commands if unspecified");
        setLineText("");
    }

    static string helpCmd(void* data, const char* name, const char* args) {
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

    void registerCommand(CommandFunc func, void *data, const char* name, const char* desc)
    {
        Command c;
        c.func         = func;
        c.data         = data;
        c.description  = desc;
        ASSERT(!commands.count(name));
        commands[str_tolower(name)] = c;
    }

    string getLineText() const
    {
        return lines[lines.size()-1].substr(2);
    }

    void setLineText(const char* text)
    {
        lines[lines.size()-1] = prompt + text;
        cursor = int2(lines[lines.size()-1].size(), lines.size()-1);
    }

    void pushCmdOutput(const char *format, ...)
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

    void saveHistory(const char *fname)
    {
        string str;
        foreach (string& line, commandHistory)
            str += line;
        SaveFile(fname, str.c_str());
    }

    void loadHistory(const char *fname)
    {
        const char* data = LoadFile(fname);
        if (data) {
            str_split(data, '\n', commandHistory);
        }
    }

    void pushHistory(const string& str)
    {
        if (commandHistory.empty() || commandHistory.back() != str)
            commandHistory.push_back(str);
        historyIndex = commandHistory.size();
    }

    bool doCommand(const string& line)
    {
        pushHistory(line);
        vector<string> expressions = str_split(str_tolower(line), ';');
        foreach (const string &expr, expressions)
        {
            vector<string> args = str_split(str_strip(expr), ' ');
            if (!args.size())
                return false;
            string cmd = args[0];
            if (args.size() == 0) {
                pushCmdOutput("");
                return false;
            }

            Command *c = NULL;
            if (map_contains(commands, cmd)) {
                c = &commands[cmd];
            } else  {
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
                                                               str_join(possible, ", ").c_str()).c_str() : "");
                    return false;
                }
            }
                 
            string argstr = str_join(args.begin() + 1, args.end(), " ");
            string ot = c->func(c->data, cmd.c_str(), argstr.c_str());
            vector<string> nlines = str_split(ot, '\n');
            lines.insert(lines.end(), nlines.begin(), nlines.end());
        }
        pushCmdOutput("");      // prompt
        return true;
    }

    bool HandleEvent(const Event* event, bool *textChanged=NULL)
    {
        if (textChanged)
            *textChanged = false;

        if (active && event->type == Event::KEY_DOWN)
        {
            switch (event->key)
            {
            case NSUpArrowFunctionKey:
                if (commandHistory.size())
                {
                    historyIndex = (historyIndex - 1) % commandHistory.size();
                    setLineText(commandHistory[historyIndex].c_str());
                }
                return true;
            case NSDownArrowFunctionKey:
                if (commandHistory.size())
                {
                    historyIndex = (historyIndex + 1) % commandHistory.size();
                    setLineText(commandHistory[historyIndex].c_str());
                }
                return true;
            case '\r':
            {
                doCommand(getLineText());
                return true;
            }
            case '\t':
            {
                string line = getLineText();
                vector<string> options;
                for (std::map<string, Command>::iterator it=commands.begin(), end=commands.end(); it != end; ++it)
                {
                    if (it->first.substr(0, line.size()) == line)
                        options.push_back(it->first);
                }

                if (options.empty()) {
                    pushText("No completions");
                } else if (options.size() == 1) {
                    setLineText(options[0].c_str());
                    if (textChanged)
                        *textChanged = true;
                } else {
                    bool done = false;
                    for (int i=line.size(); ; i++) {
                        const char c = options[0][i];
                        for (uint j=1; j<options.size(); j++) {
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
                    string s = str_join(options.begin(), options.end(), " ");
                    pushText(s.c_str());
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
        
        cursor.y = (int)lines.size()-1;
        cursor.x = clamp(cursor.x, prompt.size(), lines[cursor.y].size());

        if (lines[cursor.y].size() < prompt.size())
            setLineText("");
        
        return handled;
    }
};

struct ContextMenuBase {
    float2        position;     // upper left corner, right below title
    float2        sizePoints;   // width x height
    float2        titleSizePoints; // how much of sizePoints is for the title
    string        title;
    vector<string> lines;
    int           textSize;

    int           hovered;      // hovered line or -1 for not hovered
    bool          active;       // is it visible?
    int           selection;    // selected line index or -1 for none

    uint          defaultBGColor;
    uint          hoveredBGColor;
    uint          defaultLineColor;
    uint          hoveredLineColor;
    uint          textColor;
    uint          titleTextColor;
    
    ContextMenuBase()
    {
        title = "Menu";
        textSize = 14;

        active  = false;
        hovered = -1;

        defaultBGColor   = ALPHA(0x90)|COLOR_TEXT_BG;
        hoveredBGColor   = ALPHA(0xa0)|COLOR_BG_GRID;
        defaultLineColor = ALPHA_OPAQUE|COLOR_ENEMY;
        hoveredLineColor = ALPHA_OPAQUE|COLOR_ENEMY_HI;
        textColor        = ALPHA_OPAQUE|COLOR_UI_ACTION;
        titleTextColor   = ALPHA_OPAQUE|COLOR_ORANGE;
    }

    void setLine(int line, const char* txt)
    {
        if (line >= lines.size())
            lines.resize(line + 1);
        lines[line] = txt;
    }

    int getSelection() const { return selection; }

    float2 getCenterPos() const
    {
        const float2 sz = 0.5f * getMenuSizePoints();
        return float2(position.x + sz.x, position.y - sz.y);
    }

    float2 getMenuSizePoints() const
    {
        return float2(sizePoints.x, sizePoints.y - titleSizePoints.y);
    }

    int getHoverSelection(float2 p) const
    {
        if (lines.empty() ||  !intersectPointRectangle(p, getCenterPos(), 0.5f*sizePoints)) {
            return -1;
        }

        const float2 relp = p - position;
        const float lineHeight = getMenuSizePoints().y / lines.size();
        const int selected = (int) floor(-relp.y / lineHeight);
        return selected;
    }

    bool HandleEvent(const Event* event)
    {
        selection = -1;
        
        if (event->type != Event::MOUSE_DOWN && event->type != Event::MOUSE_UP)
            return false;

        if (active)
        {
            int selected = getHoverSelection(event->pos);
            if (event->type == Event::MOUSE_UP) {
                selection = selected;
            }
            return selected >= 0;
        }
        return false;
    }

    float2 getSizePoints() const   { return sizePoints; }

    void render(ShaderState *s_, bool selected=false)
    {
        ShaderState s = *s_;

        // calculate size
        sizePoints.x = 0;
        sizePoints.y = 0;
        for (uint i=0; i<lines.size(); i++)
        {
            const GLText* t = GLText::get(GLText::getScaledSize(textSize), lines[i].c_str());
            sizePoints.x = max(sizePoints.x, t->getSize().x);
            sizePoints.y = lines.size() * t->getSize().y;
        }
        {
            const GLText* t = GLText::get(GLText::getScaledSize(textSize), title.c_str());
            titleSizePoints = t->getSize();
            sizePoints.x = max(sizePoints.x, titleSizePoints.x);
            sizePoints.y += titleSizePoints.y;
        }
        hovered = !active ? -1 : getHoverSelection(globals.cursorPosScreen);

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

            // highlight selected element
            if (hovered == i && (hoveredBGColor&ALPHA_OPAQUE)) {
                s.color32(hoveredBGColor);
                ShaderUColor::instance().DrawRectCorners(s, position, position + float2(sizePoints.x, textHeight));
            }

            GLText::DrawStr(s, position, GLText::LEFT, textColor, textSize, lines[i]);
        }
    }
};


struct RightClickContextMenu : public ContextMenuBase {
    
    bool HandleEvent(const Event* event)
    {
        if (ContextMenuBase::HandleEvent(event)) {
            active = (getSelection() == -1); // disable on succesfull use
            return true;
        } else if (active && event->type == Event::MOUSE_DOWN) {
            active = false; // disable on click outside
        }
        
        if (event->type == Event::MOUSE_DOWN && event->key == 1) {
            position = event->pos;
            active = true; // enable on right click
            return true;
        }

        return false;
    }

    void render(ShaderState *s_, bool selected=false)
    {
        if (active) {
            ContextMenuBase::render(s_, selected);
        }
    }
};

struct ValueEditorBase {

    float2        position;     // center
    float2        sizePoints;   // width x height

    uint          textColor;
    uint          defaultBGColor;
    uint          defaultLineColor;
    uint          activeLineColor;
    
    string        prompt;
    TextInputBase edit;
    bool          editErr;

    float2 getSizePoints() const { return sizePoints; }
    
    ValueEditorBase()
    {
        textColor        = edit.textColor;
        defaultBGColor   = edit.defaultBGColor;
        defaultLineColor = edit.defaultLineColor;
        activeLineColor  = edit.activeLineColor;
        prompt           = "Value:";
        editErr          = false;

        edit.sizeChars        = int2(8, 1);
        edit.defaultBGColor   = edit.activeBGColor;
        edit.activeLineColor  = 0x0;
        edit.defaultLineColor = 0x0;
    }

    bool HandleEvent(const Event* event, bool *textChanged=NULL)
    {
        // hover over any part of the editor activates the field
        edit.forceActive = intersectPointRectangle(event->pos, position, 0.5f * sizePoints);

        return edit.HandleEvent(event, textChanged);
    }

    void render(ShaderState *s_)
    {
        ShaderState s1 = *s_;

        const GLText* glt = GLText::get(GLText::getScaledSize(edit.textSize), prompt.c_str());
        const float2  ps  = glt->getSize();

        sizePoints.x = ps.x + edit.sizePoints.x + kPadDist;
        sizePoints.y = max(ps.y, edit.sizePoints.y);
        const float2 sz = 0.5f * sizePoints;

        edit.position.x = position.x - sz.x + ps.x + kPadDist + 0.5f * edit.sizePoints.x;
        edit.position.y = position.y;

        // draw outline
        s1.translate(position);
        const uint bgColor   = defaultBGColor;
        const uint lineColor = edit.active ? activeLineColor : defaultLineColor;
        ShaderUColor::instance().DrawColorRect(s1, bgColor, sz);
        ShaderUColor::instance().DrawColorLineRect(s1, lineColor, sz);

        // draw prompt
        s1.translate(float2(-sz.x + kPadDist, -0.5f * ps.y));
        s1.color32(textColor);
        glt->render(&s1);

        edit.textColor = editErr ? 0xffff0000 : textColor;
        edit.render(s_);
    }

};


// select an option from a list of buttons
// selected button stays pressed
struct OptionButtons {

    float2         position;    // center
    float2         size;        // width x height in points
    int            selected = -1;
    vector<Button> buttons;     // button positions are relative to this position

    float2 getSizePoints() const { return size; }
    int    getSelected()   const { return selected; }

    bool HandleEvent(const Event* event, int* butActivate, int* butPress=NULL)
    {
        Event ev = *event;
        ev.pos -= position;

        bool handled = false;
        
        for (int i=0; i<buttons.size(); i++)
        {
            bool isActivate = false;
            bool isPress    = false;
            if (ButtonHandleEvent(buttons[i], &ev, "", &isActivate, &isPress))
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

    void render(ShaderState *s_, const View& view)
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
            but.render(&ss);
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
    
};

struct OptionSlider : public WidgetBase {
    
    bool        pressed = false;

    int         values  = 11;   // total number of states
    int         value   = 0;    // current state

    uint   defaultBGColor    = ALPHA(0xa0)|COLOR_TEXT_BG;
    uint   pressedBGColor    = ALPHA(0xb0)|COLOR_BG_GRID;
    uint   defaultLineColor  = ALPHA_OPAQUE|COLOR_ENEMY;
    uint   hoveredLineColor  = ALPHA_OPAQUE|COLOR_ENEMY_HI; 
    uint   inactiveLineColor = COLOR_INACTIVE;

    float2 getSizePoints() const { return size; }
    float  getValueFloat() const { return (float) value / (values-1); }
    void   setValueFloat(float v) { value = clamp((int)round(v * values), 0, values-1); }

    bool HandleEvent(const Event* event, bool *valueChanged)
    {
        const float2 sz = 0.5f*size;
        hovered = intersectPointRectangle(event->pos, position, sz);

        bool handled = hovered && ((event->type == Event::MOUSE_DOWN) ||
                                   (event->type == Event::MOUSE_UP) ||
                                   (event->type == Event::MOUSE_DRAGGED));

        pressed = hovered && ((event->type == Event::MOUSE_DOWN) ||
                              (event->type == Event::MOUSE_DRAGGED));

        if (pressed) {
            float v = ((event->pos.x - position.x) / size.x) + 0.5f;
            setValueFloat(v);
            *valueChanged = true;
        }

        return handled;
    }

    uint getBGColor() const { return pressed ? pressedBGColor : defaultBGColor; }
    uint getFGColor() const { return ((!active) ? inactiveLineColor :
                                      (hovered) ? hoveredLineColor : defaultLineColor); }
    
    void render(const ShaderState &s_)
    {
        ShaderState ss = s_;
        const float2 sz = 0.5f * size;

        ss.color(getFGColor(), alpha);
        ShaderUColor::instance().DrawLine(ss, position - float2(sz.x, 0.f), 
                                          position + float2(sz.x, 0.f));
        const float w = sz.x / values;
        DrawButton(&ss, position + float2((size.x - w) * (getValueFloat() - 0.5f), 0.f),
                   float2(w, sz.y), getBGColor(), getFGColor(), alpha);
    }
};


#endif
