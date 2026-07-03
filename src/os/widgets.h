// ClaudeOS — small immediate-ish UI toolkit shared by the apps.
#pragma once
#include <M5Cardputer.h>
#include <vector>

#include "keys.h"
#include "theme.h"

namespace ui {

// Bottom hint bar (10 px high, at the bottom of the screen).
void hintBar(M5Canvas& c, const Theme& t, const char* text);

// Centered overlay panel helper.
void overlayPanel(M5Canvas& c, const Theme& t, int w, int h, int& x, int& y);

enum IconKind : uint8_t { ICON_NONE = 0, ICON_FOLDER, ICON_FILE, ICON_WIFI, ICON_UP };

class ListView {
public:
    struct Item {
        String text;
        String right;
        uint8_t icon = ICON_NONE;
    };

    std::vector<Item> items;
    int sel   = 0;
    int rowH  = 14;

    // Handles Up/Down (and Ctrl+Up/Down for page jumps). Returns true if consumed.
    bool handleKey(const KeyEvent& e, int viewH);
    void clampSel();
    void draw(M5Canvas& c, const Theme& t, int x, int y, int w, int h);

private:
    int _scroll = 0;
};

// Modal one-line text input.
class TextPrompt {
public:
    bool active = false;

    void open(const String& label, const String& initial = "", bool mask = false);
    // Returns: 0 = still open / inactive, 1 = confirmed (value valid), -1 = cancelled.
    int handleKey(const KeyEvent& e);
    void draw(M5Canvas& c, const Theme& t);

    String value;

private:
    String _label;
    bool _mask  = false;
    int _cursor = 0;
};

// Modal message box.
class MsgBox {
public:
    enum Mode { OK, YESNO };
    bool active = false;

    void open(const String& text, Mode mode = OK);
    // Returns: 0 = still open / inactive, 1 = OK/Yes, -1 = No/cancel.
    int handleKey(const KeyEvent& e);
    void draw(M5Canvas& c, const Theme& t);

private:
    String _text;
    Mode _mode = OK;
};

String humanSize(size_t bytes);

}  // namespace ui
