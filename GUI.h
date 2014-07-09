
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
#include "Shaders.h"
#include <cctype>

static const float  kPadDist            = 2;
static const float2 kButtonPad         = float2(4.f);

#define COLOR_TARGET  0xff3a3c
#define COLOR_TEXT_BG 0x0d0715
#define COLOR_BG_GRID 0x3f1935
#define COLOR_ORANGE  0xff6f1f
#define COLOR_BLACK 0x000000

static const uint kGUIBg       = 0xb0202020;
static const uint kGUIBgActive = 0xf0404040;
static const uint kGUIFg       = 0xff404040;
static const uint kGUIFgActive = 0xfff0f0f0;
static const uint kGUIText     = 0xfff0f0f0;
static const uint kGUITextLow  = 0xffa0a0a0;
static const uint kGUIInactive = 0xa0a0a0a0;

struct WidgetBase {
    float2      position;       // center of button
    float2      size;           // width x height in points
    bool        hovered = false;
    bool        active  = true;
    float       alpha   = 1.f;

    float2 getSizePoints() const { return size; }

};

inline bool isEventKey(const Event* event, const char* keys)
{
    return event && keys && event->key < 255 && strchr(keys, event->key);
}

struct ButtonBase : public WidgetBase {
    
    const char* keys = NULL;

    bool        pressed = false;
    bool        visible = true;
    string      tooltip;

    bool HandleEvent(const Event* event, bool* isActivate, bool* isPress=NULL);
    
    bool renderTooltip(const ShaderState &ss, const View& view, uint color, bool force=false) const;

    // draw selection triangle next to selected button
    void renderSelected(const ShaderState &ss, uint bgcolor, uint linecolor, float alpha) const;
};

struct Button : public ButtonBase
{
    string text;
    float  textSize          = 24.f;
    int    textFont          = kDefaultFont;
    uint   defaultBGColor    = kGUIBg;
    uint   pressedBGColor    = kGUIBgActive;
    uint   defaultLineColor  = kGUIFg;
    uint   hoveredLineColor  = kGUIFgActive; 
    uint   inactiveLineColor = kGUIInactive;
    uint   textColor         = kGUIText;
    uint   inactiveTextColor = kGUIInactive;
    uint   style             = S_CORNERS;
    float2 padding           = float2(2.f * kPadDist, 4.f * kPadDist);

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
    
    void render(const ShaderState *s_, bool selected=false);
    void renderButton(VertexPusherTri &tri, VertexPusherLine &line);
    void renderText(const ShaderState &s_) const;
};

struct ToggleButton : public Button {

    bool enabled = false;

    bool HandleEvent(const Event* event);
};


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

    TextInputBase();
    
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

    void setText(const char* text, bool setSize=false);

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

    bool HandleEvent(const Event* event, bool *textChanged=NULL);

    void pushText(const char *format, ...);

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

    virtual void updateState(int line, ShaderState* ss){}

    void render(const ShaderState *s_);

};

struct TextInputCommandLine : public TextInputBase {

    typedef string (*CommandFunc)(void* data, const char* name, const char* args);
    typedef vector<string> (*CompleteFunc)(void* data, const char* name, const char* args);
    
    struct Command {
        string       name;
        CommandFunc  func;
        CompleteFunc comp;
        void*        data;
        string       description;
    };

    vector<string>            commandHistory;
    int                       historyIndex = 0;
    std::map<string, Command> commands;
    string                    prompt;
    
    static vector<string> comp_help(void* data, const char* name, const char* args)
    {
        vector<string> options;
        TextInputCommandLine *self = (TextInputCommandLine*) data;
        for (std::map<string, Command>::iterator it=self->commands.begin(), end=self->commands.end(); it != end; ++it)
        {
            options.push_back(it->first);
        }
        return options;
    }


    TextInputCommandLine()
    {
        sizeChars.y = 10;
        prompt = "> ";

        registerCommand(helpCmd, comp_help, this, "help", "[command]: list help for specified command, or all commands if unspecified");
        setLineText("");
    }

    static string helpCmd(void* data, const char* name, const char* args);

    void registerCommand(CommandFunc func, CompleteFunc comp, void *data, const char* name, const char* desc)
    {
        Command c;
        c.name         = name;
        c.func         = func;
        c.comp         = comp;
        c.data         = data;
        c.description  = desc;
        const string lname = str_tolower(name);
        ASSERT(!commands.count(lname));
        commands[lname] = c;
    }

    string getLineText() const
    {
        return lines[lines.size()-1].substr(prompt.size());
    }

    void setLineText(const char* text)
    {
        lines[lines.size()-1] = prompt + text;
        cursor = int2(lines[lines.size()-1].size(), lines.size()-1);
    }

    const Command *getCommand(const string &abbrev) const;

    void pushCmdOutput(const char *format, ...);
    void saveHistory(const char *fname);
    void loadHistory(const char *fname);

    void pushHistory(const string& str)
    {
        if (commandHistory.empty() || commandHistory.back() != str)
            commandHistory.push_back(str);
        historyIndex = commandHistory.size();
    }

    bool doCommand(const string& line);

    bool HandleEvent(const Event* event, bool *textChanged=NULL);
};

struct ContextMenuBase {
    float2        position;     // upper left corner, right below title
    float2        sizePoints;   // width x height
    float2        titleSizePoints; // how much of sizePoints is for the title
    string        title;
    vector<string> lines;
    int           textSize;

    int           hovered;      // hovered line or -1 for not hovered
    int           selected;     // selected line or -1 for no selection
    bool          active;       // is it visible?

    uint defaultBGColor    = ALPHA(0x90)|COLOR_TEXT_BG;
    uint hoveredBGColor    = ALPHA(0xa0)|COLOR_BG_GRID;
    uint defaultLineColor  = kGUIFg;
    uint hoveredLineColor  = kGUIFgActive;
    uint textColor         = kGUIText;
    uint selectedTextColor = kGUIText;
    uint titleTextColor    = ALPHA_OPAQUE|COLOR_ORANGE;
    
    ContextMenuBase()
    {
        title = "Menu";
        textSize = 14;

        active  = false;
        hovered = -1;
    }

    void setLine(int line, const char* txt)
    {
        if (line >= lines.size())
            lines.resize(line + 1);
        lines[line] = txt;
    }

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
        const int sel = (int) floor(-relp.y / lineHeight);
        return sel;
    }

    bool HandleEvent(const Event* event, int* select=NULL)
    {
        if (event->type != Event::MOUSE_DOWN && event->type != Event::MOUSE_UP)
            return false;

        if (active)
        {
            int sel = getHoverSelection(event->pos);
            if (event->type == Event::MOUSE_UP) {
                if (select)
                    *select = sel;
            }
            return sel >= 0;
        }
        return false;
    }

    float2 getSizePoints() const   { return sizePoints; }

    void render(ShaderState *s_);
};


struct RightClickContextMenu : public ContextMenuBase {
    
    bool HandleEvent(const Event* event, int *select)
    {
        int select1 = -1;
        if (ContextMenuBase::HandleEvent(event, &select1)) {
            active = (select1 == -1); // disable on succesfull use
            if (select)
                *select = select1;
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

    void render(ShaderState *s_)
    {
        if (active) {
            ContextMenuBase::render(s_);
        }
    }
};

struct LineSelectionBox : public WidgetBase {
    
    vector<string> lines;
    int            textSize     = 14;
    int            hoveredLine  = -1; // hovered line or -1 for not hovered
    int            selectedLine = -1; // selected line or -1 for no selection

    uint defaultBGColor    = ALPHA(0x90)|COLOR_TEXT_BG;
    uint hoveredBGColor    = ALPHA(0xa0)|COLOR_BG_GRID;
    uint defaultLineColor  = kGUIFg;
    uint hoveredLineColor  = kGUIFgActive;
    uint textColor         = kGUIText;
    uint selectedTextColor = ALPHA_OPAQUE|COLOR_TARGET;
    
    void setLine(int line, const char* txt)
    {
        if (line >= lines.size())
            lines.resize(line + 1);
        lines[line] = txt;
    }

    float getLineHeight() const { return 1.1f * GLText::getScaledSize(textSize) + 6.f; }

    int getHoverSelection(float2 p) const
    {
        if (lines.empty() ||  !intersectPointRectangle(p, position, 0.5f*size)) {
            return -1;
        }
        const float2 relp = p - (position + flipX(0.5f * size));
        const int selected = (int) floor(-relp.y / getLineHeight());
        return selected;
    }

    bool HandleEvent(const Event* event, int* select=NULL)
    {
        if (event->type != Event::MOUSE_DOWN && event->type != Event::MOUSE_UP)
            return false;

        if (active && intersectPointRectangle(event->pos, position, 0.5f*size))
        {
            int selected = getHoverSelection(event->pos);
            if (event->type == Event::MOUSE_UP) {
                selectedLine = selected;
                if (select)
                    *select = selected;
            }
            return selected >= 0;
        }
        return false;
    }

    void render(const ShaderState &ss);
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

    void render(ShaderState *s_);

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

    bool HandleEvent(const Event* event, int* butActivate, int* butPress=NULL);

    void render(ShaderState *s_, const View& view);
    
};

struct OptionSlider : public WidgetBase {
    
    bool        pressed = false;

    int         values  = 10;   // total number of states
    int         value   = 0;    // current state

    uint   defaultBGColor    = kGUIBg;
    uint   pressedBGColor    = kGUIBgActive;
    uint   defaultLineColor  = kGUIFg;
    uint   hoveredLineColor  = kGUIFgActive; 
    uint   inactiveLineColor = kGUIInactive;

    float2 getSizePoints() const { return size; }
    float  getValueFloat() const { return (float) value / (values-1); }
    void   setValueFloat(float v) { value = clamp((int)round(v * (values-1)), 0, values-1); }

    bool HandleEvent(const Event* event, bool *valueChanged);

    uint getBGColor() const { return pressed ? pressedBGColor : defaultBGColor; }
    uint getFGColor() const { return ((!active) ? inactiveLineColor :
                                      (hovered) ? hoveredLineColor : defaultLineColor); }
    
    void render(const ShaderState &s_);
};

struct TabWindow : public WidgetBase {

    struct TabButton : public ButtonBase {
        string text;

        void render(LineMesh<VertexPosColor> &lmesh, TriMesh<VertexPosColor> &tmesh) const
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
            tmesh.PushPoly(v, arraySize(v));
            lmesh.PushStrip(v, arraySize(v));
        }
    };
    
    float textSize          = 14.f;
    uint  defaultBGColor    = kGUIBg;
    uint  pressedBGColor    = kGUIBgActive;
    uint  defaultLineColor  = kGUIFg;
    uint  hoveredLineColor  = kGUIFgActive; 
    uint  inactiveLineColor = kGUIInactive;
    uint  textColor         = kGUIText;
    uint  inactiveTextColor = kGUIInactive;
    
    vector<TabButton> buttons;
    int               selected = 0;

    int addTab(string txt)
    {
        const int idx = buttons.size();
        buttons.push_back(TabButton());
        buttons.back().text = txt;
        buttons.back().keys = lstring(str_format("%d", idx)).c_str();
        return idx;
    }

    int getTab() const { return selected; }

    float getTabHeight() const { return 2.f * kPadDist + 1.5f * GLText::getScaledSize(textSize); }
    float2 getContentsCenter() const { return position - float2(0.f, 0.5f * getTabHeight()); }
    float2 getContentsSize() const { return size - float2(4.f * kPadDist) - float2(0.f, getTabHeight()); }
    float2 getContentsStart() const { return getContentsCenter() - 0.5f * getContentsSize(); }
    
    void render(const ShaderState &ss);

    bool HandleEvent(const Event* event);

};


struct ButtonLayout {

    float2 startPos;
    int2   buttonCount = int2(1, 1);
    float2 buttonSize;
    float2 buttonFootprint;

    float2 pos;
    int2   index;

    int getScalarIndex() const
    {
        return index.y * buttonCount.x + index.x;
    }

    float getButtonAlpha(float introAnim) const
    {
        const int i = getScalarIndex();
        const int count = buttonCount.x * buttonCount.y;
        return (introAnim * count > i) ? min(1.f, (introAnim * count - i)) : 0.f;
    }

    float2 getButtonPos() const
    {
        return startPos + float2(index.x+0.5f, -(index.y+0.5f)) * buttonFootprint;
    }

    // upper left corner
    void start(float2 ps)
    {
        startPos = ps;
        pos      = ps;
        index    = int2(0);
    }

    void row()
    {
        pos.x = startPos.x;
        pos.y -= buttonFootprint.y;
        index.y++;
    }

    void setTotalSize(float2 size)
    {
        setButtonFootprint(size / float2(buttonCount));
    }

    float2 getTotalSize() const
    {
        return float2(buttonCount) * buttonFootprint;
    }

    void setButtonFootprint(float2 size)
    {
        buttonFootprint = size;
        buttonSize = size - 2.f * kButtonPad;
    }

    void setButtonCount(int count, int width)
    {
        buttonCount = int2(width, (count + width - 1) / width);
    }

    void setScalarIndex(int idx)
    {
        index = int2(idx % buttonCount.x, idx / buttonCount.x);
    }

    void setupPosSize(WidgetBase *wi) const
    {
        wi->position = getButtonPos();
        wi->size = buttonSize;
    }

    void setupMultiPosSize(WidgetBase *wi, int2 slots) const
    {
        const float2 base = startPos + float2(flipY(index)) * buttonFootprint;
        wi->position = base + 0.5f * flipY(float2(slots) * buttonFootprint);
        wi->size     = float2(slots) * buttonFootprint - kButtonPad;
    }

};

struct MessageBoxWidget : public WidgetBase {

    string title = "Message";  
    string message;
    int    messageFont = kDefaultFont;
    Button okbutton;
    float  alpha2   = 1.f;

    MessageBoxWidget()
    {
        okbutton.text = "OK";
        okbutton.keys = "\r\033 ";
    }

    void updateFade();
    void render(const ShaderState &ss, const View& view);
    bool HandleEvent(const Event* event);
};

#endif
