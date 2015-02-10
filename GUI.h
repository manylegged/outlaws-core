
//
// GUI.h - widget library
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


#ifndef GUI_H
#define GUI_H

#include "GLText.h"

struct ShaderState;

static const float kPadDist = 2;
extern float2      kButtonPad;

#define COLOR_TARGET  0xff3a3c
#define COLOR_TEXT_BG 0x101010
#define COLOR_BG_GRID 0x303030
#define COLOR_ORANGE  0xff6f1f
#define COLOR_BLACK 0x000000
#define COLOR_WHITE 0xffffff

static const uint kGUIBg       = 0xb0202020;
static const uint kGUIBgActive = 0xf0404040;
static const uint kGUIFg       = 0xf0909090;
static const uint kGUIFgActive = 0xffffffff;
static const uint kGUIText     = 0xfff0f0f0;
static const uint kGUITextLow  = 0xffa0a0a0;
static const uint kGUIInactive = 0xa0a0a0a0;
static const uint kGUIToolBg   = 0xc0000000;

static const float kOverlayFGAlpha    = 0.8f;
static const float kOverlayBGAlpha    = 0.6f;
#define kOverlayBG       (ALPHAF(kOverlayBGAlpha)|COLOR_BLACK)
#define kOverlayActiveBG (ALPHAF(kOverlayFGAlpha)|COLOR_BLACK)

struct WidgetBase {
    float2      position;       // center of button
    float2      size;           // width x height in points
    bool        hovered = false;
    bool        active  = true;
    float       alpha   = 1.f;

    float2 getSizePoints() const { return size; }

};

struct ButtonBase : public WidgetBase {

    int keys[4] = {};
    string tooltip;
    bool   pressed          = false;
    bool   visible          = true;
    int    index            = -1;
    int    ident            = 0;
    uint   defaultLineColor = kGUIFg;
    uint   hoveredLineColor = kGUIFgActive;
    uint   defaultBGColor   = kGUIBg;
    float  subAlpha         = 1.f;

    ButtonBase() { }
    ButtonBase(const char* ks) { setKeys(ks); }
    virtual ~ButtonBase() {}

    void render(const ShaderState &s_, bool selected=false);
    virtual void renderButton(DMesh& mesh, bool selected=false)=0;
    virtual void renderContents(const ShaderState &s_) {};
    
    bool HandleEvent(const Event* event, bool* isActivate, bool* isPress=NULL);
    
    bool renderTooltip(const ShaderState &ss, const View& view, uint color, bool force=false) const;
    string getTooltip() const { return hovered ? tooltip : ""; }

    // draw selection triangle next to selected button
    void renderSelected(const ShaderState &ss, uint bgcolor, uint linecolor, float alpha) const;

    void setKeys(const char* ks)
    {
        if (!ks || *ks == '\0')
            return;
        const int len = strlen(ks);
        ASSERT(ks && isBetween(len, 1, (int)arraySize(keys)));
        for (int i = 0; i < arraySize(keys); i++) {
            keys[i] = i < len ? ks[i] : 0;
        }
    }

    void setKeys(std::initializer_list<uint> lst)
    {
        ASSERT(isBetween((int)lst.size(), 1, (int)arraySize(keys)));
        for (int i = 0; i < arraySize(keys); i++) {
            keys[i] = i < lst.size() ? *(lst.begin() + i) : 0;
        }
    }

};

struct Button : public ButtonBase
{
    string text;
    string subtext;
    float  textSize          = 24.f;
    int    textFont          = kDefaultFont;
    float  subtextSize       = 16.f;
    uint   subtextColor      = kGUITextLow;
    uint   pressedBGColor    = kGUIBgActive;
    uint   inactiveLineColor = kGUIInactive;
    uint   textColor         = kGUIText;
    uint   inactiveTextColor = kGUIInactive;
    uint   style             = S_CORNERS;
    float2 padding           = float2(4.f * kPadDist, 4.f * kPadDist);

    Button() {}
    Button(const string& str, const char* keys=NULL, int ky=0) : ButtonBase(keys), text(str) { ident = ky; }

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
    float2 getTextSize() const;
    virtual void renderButton(DMesh& mesh, bool selected=false);
    virtual void renderContents(const ShaderState &s_);
    
    void setEscapeKeys() { setKeys({ EscapeCharacter, GamepadB });
                           subtext = KeyState::instance().stringNo(); }
    void setReturnKeys() { setKeys({ EscapeCharacter, '\r', GamepadA, GamepadB });
                           subtext = KeyState::instance().stringYes(); }
    void setYesKeys()    { setKeys({ '\r', GamepadA });
                           subtext = KeyState::instance().stringYes(); }
    void setNoKeys()     { setKeys({ EscapeCharacter, GamepadB });
                           subtext = KeyState::instance().stringNo(); }
    void setDiscardKeys(){ setKeys({ NSDeleteFunctionKey, NSBackspaceCharacter, GamepadY });
                           subtext = KeyState::instance().stringDiscard(); }

};


struct Scrollbar : public WidgetBase {

    int         first   = 0;    // first visible item
    int         lines   = 0;    // number of visible items
    int         steps   = 0;    // total items
    bool        pressed = false; // is actively dragging thumb?
    float       sfirst  = 0.f;  // float version of first for scrolling
    WidgetBase *parent  = NULL;

    uint        defaultBGColor = kGUIBg;
    uint        pressedBGColor = kGUIBgActive;
    uint        defaultFGColor = kGUIFg;
    uint        hoveredFGColor = kGUIFgActive;

    int last() const { return min(first + lines, steps); }
    void render(DMesh &mesh) const;
    bool HandleEvent(const Event *event);
};


struct TextInputBase : public WidgetBase {
    
    deque<string>        lines;
    std::recursive_mutex mutex; // for lines
    float                textSize  = 12;
    bool                 fixedSize = false;
    Scrollbar            scrollbar;
    
    int2 sizeChars = int2(80, 2);
    int2 startChars;
    int2 cursor;
    bool active      = false;   // is currently editable?
    bool locked      = false;   // set to true to disable editing
    bool forceActive = false;   // set to true to enable editing regardless of mouse position

    uint  defaultBGColor   = ALPHA(0.5f)|COLOR_TEXT_BG;
    uint  activeBGColor    = ALPHA(0.65)|COLOR_BG_GRID;
    uint  defaultLineColor = kGUIFg;
    uint  activeLineColor  = kGUIFgActive;
    uint  textColor        = kGUIText;

    TextInputBase()
    {
        lines.push_back(string());
    }
    
    ~TextInputBase() {}

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
    void setLines(const vector<string> &lines);

    bool HandleEvent(const Event* event, bool *textChanged=NULL);

    void popText(int chars);          // remove at end
    void pushText(const char *txt, int linesback=1);   // insert at end
    void pushText(const string& str, int linesback=1) { pushText(str.c_str(), linesback); }
    void insertText(const char *txt); // insert at cursor

    void scrollForInput()
    {
        startChars.y = max(0, (int)lines.size() - sizeChars.y);
    }

    float2 getCharSize() const;
    int2 getSizeChars() const    { return sizeChars; }
    float2 getSizePoints() const { return size; }

    void render(const ShaderState &s_);
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
    string                    lastSearch;
    int                       historyIndex = 0;
    std::map<string, Command> commands;
    string                    prompt = "^2>^7 ";
    
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
        std::lock_guard<std::recursive_mutex> l(mutex);
        lines[lines.size()-1] = prompt + text;
        cursor = int2(lines[lines.size()-1].size(), lines.size()-1);
    }

    const Command *getCommand(const string &abbrev) const;

    void pushCmdOutput(const char *format, ...) __printflike(2, 3);
    void saveHistory(const char *fname);
    void loadHistory(const char *fname);

    void pushHistory(const string& str)
    {
        if (commandHistory.empty() || commandHistory.back() != str)
            commandHistory.push_back(str);
        historyIndex = (int)commandHistory.size();
    }

    bool doCommand(const string& line);
    bool pushCommand(const string& line);

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

    uint defaultBGColor    = ALPHA(0.5f)|COLOR_TEXT_BG;
    uint hoveredBGColor    = ALPHA(0.65f)|COLOR_BG_GRID;
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

    int         hoveredValue = -1;

    uint   defaultBGColor    = kGUIBg;
    uint   pressedBGColor    = kGUIBgActive;
    uint   defaultLineColor  = kGUIFg;
    uint   hoveredLineColor  = kGUIFgActive; 
    uint   inactiveLineColor = kGUIInactive;

    float2 getSizePoints() const { return size; }
    float  getValueFloat() const { return (float) value / (values-1); }
    int    floatToValue(float v) const { return clamp(floor_int(v * values), 0, values-1); }
    void   setValueFloat(float v) { value = floatToValue(v); }

    bool HandleEvent(const Event* event, bool *valueChanged);

    uint getBGColor() const { return pressed ? pressedBGColor : defaultBGColor; }
    uint getFGColor() const { return ((!active) ? inactiveLineColor :
                                      (hovered) ? hoveredLineColor : defaultLineColor); }

    bool isDiscrete() const { return values < 5; }
    
    void render(const ShaderState &s_);
};


// edit a float or int type, with a label
struct OptionEditor {

    enum Type { FLOAT, INT };

    OptionSlider slider;
    const char*  label   = NULL;
    vector<const char*> tooltip;
    Type         type;
    void*        value = NULL;
    float        start;
    float        mult;
    string       txt;

    float getValueFloat() const
    {
        return ((type == FLOAT) ? *(float*) value : ((float) *(int*) value));
    }

    void setValueFloat(float v);
    int getValueInt() const { return *(int*) value; }

    void updateSlider()
    {
        slider.setValueFloat((getValueFloat() - start) / mult);
        txt = str_format("%s: %s", label, getTxt().c_str());
    }

    void init(Type t, void *v, const char* lbl, const vector<const char*> &tt, float st, float mu, int states);
    
    OptionEditor(float *f, const char* lbl, float mn, float mx, const vector<const char*> tt) 
    {
        init(FLOAT, (void*) f, lbl, tt, mn, mx, 100);
    }

    OptionEditor(int *u, const char* lbl, int states, const vector<const char*> tt)
    {
        init(INT, (void*) u, lbl, tt, 0.f, (float) states-1, states);
    } 

    OptionEditor(int *u, const char* lbl, int low, int increment, int states, const vector<const char*> tt)
    {
        init(INT, (void*) u, lbl, tt, low, increment * states, states + 1);
    }

    string getTxt() const;
    float2 render(const ShaderState &ss, float alpha);
    bool HandleEvent(const Event* event, bool* valueChanged);
};



struct ColorPicker : public WidgetBase {

    OptionSlider hueSlider;
    uint         initialColor;

    float2       svRectSize;
    float2       svRectPos;
    bool         svDragging = false;
    bool         svHovered = false;

    float3       hsvColor;

    ColorPicker(uint initial=0)
    {
        hueSlider.values = 360;
        setInitialColor(initial);
    }

    void setInitialColor(uint initial)
    {
        hsvColor = rgb2hsvf(initial);
        hueSlider.setValueFloat(hsvColor.x / 360.f);
        initialColor = initial;
    }
    
    uint getColor() const { return hsvf2rgb(hsvColor); }

    void render(const ShaderState &s_);
    bool HandleEvent(const Event* event, bool *valueChanged=NULL);
    
};

struct ITabInterface {

    virtual bool HandleEvent(const Event* event)=0;
    virtual void renderTab(float2 center, float2 size, float foreground, float introAnim)=0;
    virtual bool onSwapOut() { return true; }
    virtual void onSwapIn() {}
    virtual void onStep() {}
    
    virtual ~ITabInterface(){}
    
};


struct TabWindow : public WidgetBase {

    struct TabButton final : public ButtonBase {
        string         text;
        ITabInterface *interface = NULL;
        int            ident     = -1;

        void renderButton(DMesh &mesh, bool selected=false);
    };
    
    float textSize          = 16.f;
    uint  inactiveBGColor   = kGUIBgActive;
    uint  defaultBGColor    = kGUIBg;
    uint  defaultLineColor  = kGUIFg;
    uint  hoveredLineColor  = kGUIFgActive; 
    uint  inactiveLineColor = kGUIInactive;
    uint  textColor         = kGUIText;

    vector<TabButton> buttons;
    int               selected = 0;
    float             alpha2 = 1.f;

    int addTab(string txt, int ident, ITabInterface *inf);

    int getTab() const { return selected; }
    ITabInterface* getActive() { return buttons[selected].interface; }

    float getTabHeight() const;
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
        return (introAnim * count > i) ? min(introAnim, (introAnim * count - i)) : 0.f;
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

    void flowCountTotalSize(int count, float2 tsize)
    {
        if (count > 0)
        {
            buttonCount = int2(0);
            float2 bsize;
            do {
                buttonCount.x++;
                buttonCount.y = ceil_int(float(count) / buttonCount.x);
                bsize = tsize / float2(buttonCount);
            } while (bsize.x > 2.f * bsize.y);
        }
        setTotalSize(tsize);
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
        okbutton.setReturnKeys();
    }

    void updateFade();
    void render(const ShaderState &ss, const View& view);
    bool HandleEvent(const Event* event);
};

struct TextBox {
    
    const View* view = NULL;
    float2      rad;
    float2      box;
    uint        fgColor = kGUIText;
    uint        bgColor = kGUIToolBg;
    int         font    = kMonoFont;
    float       tSize   = 12.f;
    float       alpha   = 1.f;
    
    void Draw(const ShaderState& ss1, float2 point, const string& text) const;
};

struct OverlayMessage : public WidgetBase {
    std::mutex mutex;
    string     message;
    float      startTime = 0.f;
    float      totalTime = 1.;
    uint       color     = kGUIText;
    float      textSize  = 14.f;
    GLText::Align align = GLText::MID_CENTERED;
    
    bool isVisible() const;
    bool setMessage(const string& msg, uint clr=0); // return true if changed
    void setVisible(bool visible=true);
    void render(const ShaderState &ss);
    
    void reset()
    {
        startTime = 0.f;
        message.clear();
    }
};

// do a 'press X to delete', then 'press X to confirm' kind of thing
bool HandleConfirmKey(const Event *event, int* slot, int selected, bool *sawUp,
                      int key0, int key1, bool *isConfirm);

// move selected button around in a group of buttons using cursor keys or gamepad
bool HandleEventSelected(int* selected, ButtonBase &current, int count, int rows, 
                         const Event* event, bool* isActivate);

// button helper, with sound effects
bool ButtonHandleEvent(ButtonBase &button, const Event* event, bool* isActivate,
                       bool* isPress=NULL, int* selected=NULL);

// auto-resizing text
struct ButtonText {
    float fontSize = -1.f;
    
    void renderText(const ShaderState &ss, float2 pos, float width,
                    GLText::Align align, uint color, float fmin, float fmax,
                    const string& text);
};

// scrolling button container
struct ButtonWindow : public WidgetBase {

    vector<ButtonBase *> buttons;
    Scrollbar            scrollbar;
    int2                 dims = int2(2, 8);

    ButtonWindow()
    {
        scrollbar.parent = this;
    }

    ~ButtonWindow()
    {
        vec_clear_deep(buttons);
    }
    
    void render(const ShaderState &ss);
    bool HandleEvent(const Event *event, ButtonBase **activated=NULL,
                     ButtonBase **dragged=NULL, ButtonBase **dropped=NULL);

    void computeDims(int2 mn, int2 mx);
};


#endif
