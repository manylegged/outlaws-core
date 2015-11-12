
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
static const uint kGUIFgMid    = 0xf0b8b8b8;
static const uint kGUIFgActive = 0xffffffff;
static const uint kGUIText     = 0xfff0f0f0;
static const uint kGUITextLow  = 0xff808080;
static const uint kGUIInactive = 0xa0606060;
static const uint kGUIToolBg   = 0xc0000000;

static const float kOverlayFGAlpha    = 0.8f;
static const float kOverlayBGAlpha    = 0.6f;
#define kOverlayBG       (ALPHAF(kOverlayBGAlpha)|COLOR_BLACK)
#define kOverlayActiveBG (ALPHAF(kOverlayFGAlpha)|COLOR_BLACK)

struct WidgetBase {
    float2      position;       // center of button
    float2      size;           // width x height in points
    int         hovered = false;
    bool        active  = true;
    float       alpha   = 1.f;

    float2 getSizePoints() const { return size; }

    void setAdjacent(const WidgetBase &last, float2 rpos)
    {
        position = last.position + (last.size + size + 4.f * kButtonPad) * (rpos / 2.f);
    }

    void setAboveBelow(const WidgetBase &last, float2 rpos)
    {
        position = last.position + (last.size + flipX(size) + justY(4.f * kButtonPad)) * (rpos / 2.f);
    }
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
    
    virtual bool HandleEvent(const Event* event, bool* isActivate, bool* isPress=NULL);
    
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

    void setKeys(int *vec, int count)
    {
        for (int i = 0; i < arraySize(keys); i++) {
            keys[i] = i < count ? vec[i] : 0;
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

    float dynamicTextSize    = 0.f;
    float dynamicSubtextSize = 0.f;

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

struct URLButton : public Button {
    string url;
    URLButton() : Button() {}
    URLButton(const string &name, const string &u) : Button(name), url(u) {}
    bool HandleEvent(const Event* event, bool *isActivate, bool *isPress=NULL);
};


struct Scrollbar final : public WidgetBase{

    int         first   = 0;    // first visible item
    int         visible   = 0;    // number of visible items
    int         total   = 0;    // total items
    bool        pressed = false; // is actively dragging thumb?
    bool        moved = false;   // scrolled window, needs to recompute hovered
    float       sfirst  = 0.f;  // float version of first for scrolling
    WidgetBase *parent  = NULL;

    uint        defaultBGColor = kGUIBg;
    uint        defaultFGColor = kGUIFg;
    uint        hoveredFGColor = kGUIFgMid;
    uint        pressedFGColor = kGUIFgActive;

    int last() const { return min(first + visible, total); }
    void render(DMesh &mesh);
    bool HandleEvent(const Event *event);
    void makeVisible(int row);

    Rect2d thumb() const;
};


struct TextInputBase : public WidgetBase {
    
    deque<string>        lines;
    std::recursive_mutex mutex; // for lines
    float                textSize  = 12;
    bool                 fixedWidth = true;
    bool                 fixedHeight = true;
    bool                 wrapText = false;
    Scrollbar            scrollbar;
    
    int2 sizeChars = int2(20, 1);
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
    
    string str() { return getText(); }

    void setText(const char* text);
    void setLines(const vector<string> &lines);

    bool HandleEvent(const Event* event, bool *textChanged=NULL);

    void popText(int chars);          // remove at end
    void pushText(string str, int linesback=1);
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
        void*        data = NULL;
        string       description;
    };

    vector<string>            commandHistory;
    string                    currentCommand;
    string                    lastSearch;
    int                       historyIndex = 0;
    std::map<string, Command> commands;
    string                    prompt = "^2>^7 ";
    
    static vector<string> comp_help(void* data, const char* name, const char* args);

    TextInputCommandLine();

    vector<string> completeCommand(string cmd) const;
    static string cmd_help(void* data, const char* name, const char* args);
    static string cmd_find(void* data, const char* name, const char* args);

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
        const string &line = lines[lines.size()-1];
        return line.size() > prompt.size() ? line.substr(prompt.size()) : string();
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

struct ContextMenu : public WidgetBase {
    // position is upper left corner, right below title
    vector<string> lines;
    vector<bool>   enabled;
    float          textSize = 16;
    double         openTime = 0.f;

    uint defaultBGColor    = 0xf0202020;
    uint hoveredBGColor    = kGUIBgActive;
    uint defaultLineColor  = kGUIFgActive;
    uint textColor         = kGUIText;
    uint inactiveTextColor = kGUITextLow;

    ContextMenu() { active = false; hovered = -1; }
    void setLine(int line, const string &txt);
    float2 getCenterPos() const { return position + flipY(size / 2.f); }
    int getHoverSelection(float2 p) const;
    void openMenu(float2 pos);
    bool HandleEvent(const Event* event, int* select=NULL);
    void render(const ShaderState &s_);
};

struct BContext : public Button {
    vector<string> vals;
    ContextMenu    menu;
    string         title;
    string         selection;

    BContext();
    void pushItem(const string &key, const string &val);
    void setSelection(int index);
    bool HandleEventMenu(const Event* event, bool* selectionChanged);
    void renderContents(const ShaderState &ss);
private:
    using ButtonBase::HandleEvent;
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

// must set size!
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
    bool   allowBinary       = true;


    float2 getSizePoints() const { return size; }
    float  getValueFloat() const { return (float) value / (values-1); }
    int    floatToValue(float v) const { return clamp(floor_int(v * values), 0, values-1); }
    void   setValueFloat(float v) { value = floatToValue(v); }

    bool HandleEvent(const Event* event, bool *valueChanged);

    uint getBGColor() const { return pressed ? pressedBGColor : defaultBGColor; }
    uint getFGColor() const { return ((!active) ? inactiveLineColor :
                                      (hovered) ? hoveredLineColor : defaultLineColor); }

    bool isBinary() const { return allowBinary && values == 2; }
    bool isDiscrete() const { return values < 5; }
    
    void render(const ShaderState &s_);
};


// edit a float or int type, with a label
struct OptionEditor {

    enum Type { FLOAT, INT };
    enum Format { DEFAULT, SECONDS, COUNT };

    OptionSlider slider;
    const char*  label   = NULL;
    vector<const char*> tooltip;
    Type         type;
    Format       format = DEFAULT;
    void*        value = NULL;
    float        start = 0.f;
    float        mult = 1.f;
    string       txt;

    float getValueFloat() const;
    void setValueFloat(float v);
    int getValueInt() const { return *(int*) value; }

    void updateSlider();
    void setFormat(Format fm) { format = fm; updateSlider(); }

    typedef vector<const char*> StringVec;
    
    void init(Type t, void *v, const char* lbl, const StringVec &tt, float st, float mu, int states);

    OptionEditor(float *f, const char* lbl, float mn, float mx, const StringVec tt=StringVec()) ;
    OptionEditor(float *f, const char* lbl, float mn, float mx, float inc, const StringVec tt=StringVec());
    OptionEditor(int *u, const char* lbl, int states, const StringVec tt=StringVec());
    OptionEditor(int *u, const char* lbl, int low, int high, int increment, const StringVec tt=StringVec());

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

    bool m_isActiveTab = false;

    virtual bool HandleEvent(const Event* event)=0;
    virtual void renderTab(const View &view)=0;
    virtual void onSwapOut() { }
    virtual void onSwapIn() {}
    virtual void onStep() {}
    
    virtual ~ITabInterface(){}
    
};


struct KeyBinding;

struct TabWindow : public WidgetBase {

    struct TabButton final : public ButtonBase {
        const KeyBinding *key = NULL;
        string            text;
        ITabInterface    *interface = NULL;
        int               ident     = -1;

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

    int addTab(string txt, int ident, ITabInterface *inf, const KeyBinding *key=NULL);

    int getTab() const { return selected; }
    ITabInterface* getActive() { return buttons[selected].interface; }

    float getTabHeight() const;
    float2 getContentsCenter() const { return position - float2(0.f, 0.5f * getTabHeight()); }
    float2 getContentsSize() const { return size - float2(4.f * kPadDist) - float2(0.f, getTabHeight()); }
    float2 getContentsStart() const { return getContentsCenter() - 0.5f * getContentsSize(); }
    
    void render(const ShaderState &ss, const View &view);

    bool HandleEvent(const Event* event, bool *istoggle=NULL);

    bool swapToTab(int next);
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
        index.x = 0;
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

struct MessageBoxBase : public WidgetBase {
    
    string title;
    string message;
    int    messageFont = kDefaultFont;
    float  titleSize = 36.f;
    float  textSize = 16.f;
    float  alpha2   = 1.f;

    Button okbutton = Button(_("OK"));

    void updateFade();
    void render(const ShaderState &ss, const View& view);
};

struct MessageBoxWidget : public MessageBoxBase {

    MessageBoxWidget();

    void render(const ShaderState &ss, const View& view);
    bool HandleEvent(const Event* event);
};

struct ConfirmWidget : public MessageBoxBase {

    Button cancelbutton;

    ConfirmWidget();
    
    void render(const ShaderState &ss, const View& view);
    bool HandleEvent(const Event* event, bool *selection);
};

struct ScrollMessageBox : public WidgetBase {

    string        title;
    TextInputBase message;
    
    Button okbutton = Button(_("OK"));

    ScrollMessageBox();
    void render(const ShaderState &ss, const View &view);
    bool HandleEvent(const Event *event);

    void activateSetText(const char* txt);
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

struct TextBoxString {
    TextBox box;
    float2 position;
    string text;

    void Draw(const ShaderState& ss1) const
    {
        box.Draw(ss1, position, text);
    }
};

struct OverlayMessage : public WidgetBase {
    std::mutex mutex;
    string     message;
    float      startTime = 0.f;
    float      totalTime = 1.f;
    uint       color     = kGUIText;
    int        font      = kDefaultFont;
    float      textSize  = 14.f;
    GLText::Align align = GLText::MID_CENTERED;
    bool       border = false;
    
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


float2 renderButtonText(const ShaderState &ss, float2 pos, float width,
                        GLText::Align align, int font, uint color, float *fontSize, float fmin, float fmax,
                        const string& text);

// auto-resizing text
struct ButtonText {
    float fontSize = -1.f;
    
    float2 renderText(const ShaderState &ss, float2 pos, float width,
                      GLText::Align align, uint color, float fmin, float fmax,
                      const string& text)
    {
        return renderButtonText(ss, pos, width, align, kDefaultFont, color, &fontSize, fmin, fmax, text);
    }
};

// scrolling button container
struct ButtonWindowBase : public WidgetBase {

    vector<ButtonBase *> buttons;
    Scrollbar            scrollbar;
    int2                 dims = int2(2, 8);
    bool                 rearrange = false;

    ButtonBase *         extDragPtr = NULL; // don't draw this one, will be drawn externally

    ButtonWindowBase()
    {
        scrollbar.parent = this;
    }

    ~ButtonWindowBase()
    {
        vec_clear_deep(buttons);
    }
    
    void render(const ShaderState &ss);
    bool HandleEvent(const Event *event, ButtonBase **activated=NULL,
                     ButtonBase **dragged=NULL, ButtonBase **dropped=NULL);
    ButtonBase *HandleRearrange(const Event *event, ButtonBase *dragged);
    void swapButtons(ButtonBase *a, ButtonBase *b);
    bool HandleDragExternal(const Event *event, ButtonBase *dragged, ButtonBase **drop=NULL);

    void computeDims(int2 mn, int2 mx);
    void scrollfor(int idx) { scrollbar.makeVisible(idx / dims.x); }

    void popButton(ButtonBase *but);
    
protected:
    std::mutex           mutex;
    float2               dragOffset;     // positon of dragged button relative to mouse pointer
    float2               dragPos;        // original position of dragged button
    ButtonBase **        dragPtr = NULL; // pointer to dragged button in buttons vector

    ButtonBase **getPtr(ButtonBase *but);
    bool setupDragPtr(const Event *event, ButtonBase *dragged);
};

template <typename T>
struct ButtonWindow final : public ButtonWindowBase {

    pair<T**, T**> button_iter()
    {
        if (buttons.empty())
            return make_pair((T**)NULL, (T**)NULL);
        T** first = (T**)&buttons[0];
        return make_pair(first, first + buttons.size());
    }

    pair<const T**, const T**> button_iter() const
    {
        if (buttons.empty())
            return make_pair((const T**)NULL, (const T**)NULL);
        const T** first = (const T**)&buttons[0];
        return make_pair(first, first + buttons.size());
    }

    T* getDragged() const
    {
        return dragPtr ? static_cast<T*>(*dragPtr) : NULL;
    }

    T *getDropped(const Event *event) const
    {
        return (event->type == Event::MOUSE_UP && dragPtr) ? static_cast<T*>(*dragPtr) : NULL;
    }

    T *consumeDropped(const Event *event)
    {
        T* dropd = getDropped(event);
        if (dropd) {
            dropd->pressed = false;
            dropd->hovered = false;
            dragPtr = NULL;
        }
        return dropd;
    }

    void pushButton(T *but)
    {
        std::lock_guard<std::mutex> l(mutex);
        but->index = buttons.size();
        buttons.push_back(but);
    }

    bool HandleDragExternal(const Event *event, T *dragged, T **drop=NULL)
    {
        return ButtonWindowBase::HandleDragExternal(event, (ButtonBase*)dragged, (ButtonBase**)drop);
    }

    bool HandleEvent(const Event *event, T **activated=NULL, T **dragged=NULL, T **dropped=NULL)
    {
        return ButtonWindowBase::HandleEvent(event, (ButtonBase**)activated, (ButtonBase**)dragged, (ButtonBase**)dropped);
    }

    T *HandleRearrange(const Event *event, T *dragged)
    {
        return (T*)ButtonWindowBase::HandleRearrange(event, dragged);
    }
};

// similar to above but allows keyboard selection, less rearrangement, no outline
struct ButtonSelector : public WidgetBase {

    vector<ButtonBase *> buttons;
    Scrollbar            scrollbar;
    int2                 dims = int2(3, 3);
    int                  selected = 0;

    void render(const ShaderState &ss);
    bool HandleEvent(const Event *event, int *pressed);

    template <typename T>
    void setButtons(T *ptr, uint count)
    {
        for (int i=0; i<count; i++)
        {
            ptr[i]->index = i;
            buttons.push_back(ptr[i]);
        }
        scrollbar.visible = dims.y;
        scrollbar.total = (count + dims.x-1) / dims.x;
        scrollbar.active = scrollbar.total >= count;
    }
};


#endif
