#include "widgets.h"

#include "os.h"

namespace ui {

void hintBar(M5Canvas& c, const Theme& t, const char* text)
{
    const int h = 10;
    c.fillRect(0, SCREEN_H - h, SCREEN_W, h, t.panelHi);
    c.setFont(&fonts::Font0);
    c.setTextSize(1);
    c.setTextDatum(textdatum_t::middle_left);
    c.setTextColor(t.dim, t.panelHi);
    c.drawString(text, 4, SCREEN_H - h / 2);
}

void overlayPanel(M5Canvas& c, const Theme& t, int w, int h, int& x, int& y)
{
    x = (SCREEN_W - w) / 2;
    y = (SCREEN_H - h) / 2;
    c.fillRoundRect(x + 2, y + 3, w, h, 6, C565(0, 0, 0));  // soft shadow
    c.fillRoundRect(x, y, w, h, 6, t.panel);
    c.drawRoundRect(x, y, w, h, 6, t.accent);
}

String humanSize(size_t bytes)
{
    char buf[24];
    if (bytes < 1024) {
        snprintf(buf, sizeof(buf), "%uB", (unsigned)bytes);
    } else if (bytes < 1024UL * 1024UL) {
        snprintf(buf, sizeof(buf), "%.1fK", bytes / 1024.0);
    } else if (bytes < 1024UL * 1024UL * 1024UL) {
        snprintf(buf, sizeof(buf), "%.1fM", bytes / (1024.0 * 1024.0));
    } else {
        snprintf(buf, sizeof(buf), "%.1fG", bytes / (1024.0 * 1024.0 * 1024.0));
    }
    return String(buf);
}

// --- ListView ---------------------------------------------------------------

void ListView::clampSel()
{
    if (items.empty()) {
        sel = 0;
        _scroll = 0;
        return;
    }
    if (sel < 0) sel = 0;
    if (sel >= (int)items.size()) sel = items.size() - 1;
}

bool ListView::handleKey(const KeyEvent& e, int viewH)
{
    if (items.empty()) return false;
    int page = viewH / rowH;
    if (page < 1) page = 1;

    switch (e.key) {
        case Key::Up:
            sel += e.ctrl ? -page : -1;
            break;
        case Key::Down:
            sel += e.ctrl ? page : 1;
            break;
        default:
            return false;
    }
    if (sel < 0) sel = 0;
    if (sel >= (int)items.size()) sel = items.size() - 1;
    return true;
}

static void drawIcon(M5Canvas& c, const Theme& t, uint8_t kind, int x, int y, bool selected)
{
    uint16_t col = selected ? t.selText : t.accent;
    switch (kind) {
        case ICON_FOLDER:
            c.fillRect(x, y + 2, 4, 2, col);
            c.fillRect(x, y + 3, 9, 6, col);
            break;
        case ICON_FILE:
            c.drawRect(x + 1, y + 1, 7, 9, selected ? t.selText : t.dim);
            c.drawFastHLine(x + 3, y + 4, 3, selected ? t.selText : t.dim);
            c.drawFastHLine(x + 3, y + 6, 3, selected ? t.selText : t.dim);
            break;
        case ICON_WIFI:
            c.fillCircle(x + 4, y + 8, 1, col);
            c.drawArc(x + 4, y + 8, 4, 4, 225, 315, col);
            c.drawArc(x + 4, y + 8, 7, 7, 225, 315, col);
            break;
        case ICON_UP:
            c.fillTriangle(x + 4, y + 2, x + 1, y + 7, x + 7, y + 7, col);
            break;
        default:
            break;
    }
}

void ListView::draw(M5Canvas& c, const Theme& t, int x, int y, int w, int h)
{
    clampSel();
    int visible = h / rowH;
    if (visible < 1) visible = 1;
    if (sel < _scroll) _scroll = sel;
    if (sel >= _scroll + visible) _scroll = sel - visible + 1;
    if (_scroll < 0) _scroll = 0;

    c.setFont(&fonts::Font0);
    c.setTextSize(1);

    for (int row = 0; row < visible; row++) {
        int idx = _scroll + row;
        if (idx >= (int)items.size()) break;
        const Item& it = items[idx];
        int ry         = y + row * rowH;
        bool selected  = (idx == sel);
        if (selected) {
            c.fillRoundRect(x, ry, w, rowH - 1, 3, t.sel);
        }
        uint16_t fg = selected ? t.selText : t.fg;
        uint16_t bgc = selected ? t.sel : t.bg;
        int tx = x + 4;
        if (it.icon != ICON_NONE) {
            drawIcon(c, t, it.icon, x + 3, ry + 1, selected);
            tx = x + 16;
        }
        c.setTextDatum(textdatum_t::middle_left);
        c.setTextColor(fg, bgc);
        String txt   = it.text;
        int maxTextW = w - (tx - x) - (it.right.isEmpty() ? 4 : c.textWidth(it.right) + 10);
        while (txt.length() > 1 && (int)c.textWidth(txt) > maxTextW) {
            txt.remove(txt.length() - 1);
        }
        c.drawString(txt, tx, ry + rowH / 2);
        if (!it.right.isEmpty()) {
            c.setTextDatum(textdatum_t::middle_right);
            c.setTextColor(selected ? t.selText : t.dim, bgc);
            c.drawString(it.right, x + w - 4, ry + rowH / 2);
        }
    }

    // scrollbar
    if ((int)items.size() > visible) {
        int barH = h * visible / items.size();
        if (barH < 8) barH = 8;
        int barY = y + (h - barH) * _scroll / (items.size() - visible);
        c.fillRoundRect(x + w - 2, y, 2, h, 1, t.panel);
        c.fillRoundRect(x + w - 2, barY, 2, barH, 1, t.accent);
    }
}

// --- TextPrompt ---------------------------------------------------------------

void TextPrompt::open(const String& label, const String& initial, bool mask)
{
    active  = true;
    _label  = label;
    value   = initial;
    _mask   = mask;
    _cursor = value.length();
}

int TextPrompt::handleKey(const KeyEvent& e)
{
    if (!active) return 0;
    switch (e.key) {
        case Key::Enter:
            active = false;
            return 1;
        case Key::Esc:
            active = false;
            return -1;
        case Key::Backspace:
            if (_cursor > 0) {
                value.remove(_cursor - 1, 1);
                _cursor--;
            }
            break;
        case Key::Left:
            if (_cursor > 0) _cursor--;
            break;
        case Key::Right:
            if (_cursor < (int)value.length()) _cursor++;
            break;
        case Key::Char:
            if (value.length() < 120 && !e.fn) {
                value = value.substring(0, _cursor) + String(e.ch) + value.substring(_cursor);
                _cursor++;
            }
            break;
        default:
            break;
    }
    return 0;
}

void TextPrompt::draw(M5Canvas& c, const Theme& t)
{
    if (!active) return;
    int x, y;
    const int w = 216, h = 52;
    overlayPanel(c, t, w, h, x, y);
    c.setFont(&fonts::Font0);
    c.setTextSize(1);
    c.setTextDatum(textdatum_t::top_left);
    c.setTextColor(t.accent, t.panel);
    c.drawString(_label, x + 8, y + 6);

    // input field
    c.fillRoundRect(x + 8, y + 18, w - 16, 14, 2, t.bg);
    String shown = value;
    if (_mask) {
        shown = "";
        for (size_t i = 0; i < value.length(); i++) shown += '*';
    }
    // keep cursor visible: show tail if too long
    int maxChars = (w - 24) / 6;
    int start    = 0;
    if (_cursor > maxChars - 1) start = _cursor - (maxChars - 1);
    String vis = shown.substring(start, min((int)shown.length(), start + maxChars));
    c.setTextColor(t.fg, t.bg);
    c.drawString(vis, x + 12, y + 21);
    // caret
    int cx = x + 12 + (_cursor - start) * 6;
    if ((millis() / 400) & 1) c.fillRect(cx, y + 20, 1, 10, t.accent);

    c.setTextColor(t.dim, t.panel);
    c.drawString("Enter: OK   Esc: cancel", x + 8, y + 38);
}

// --- MsgBox ---------------------------------------------------------------

void MsgBox::open(const String& text, Mode mode)
{
    active = true;
    _text  = text;
    _mode  = mode;
}

int MsgBox::handleKey(const KeyEvent& e)
{
    if (!active) return 0;
    if (_mode == OK) {
        if (e.key == Key::Enter || e.key == Key::Esc ||
            (e.key == Key::Char && (e.ch == 'o' || e.ch == ' '))) {
            active = false;
            return 1;
        }
        return 0;
    }
    // YESNO
    if (e.key == Key::Char && (e.ch == 'y' || e.ch == 'Y')) {
        active = false;
        return 1;
    }
    if (e.key == Key::Enter) {
        active = false;
        return 1;
    }
    if (e.key == Key::Esc || (e.key == Key::Char && (e.ch == 'n' || e.ch == 'N'))) {
        active = false;
        return -1;
    }
    return 0;
}

void MsgBox::draw(M5Canvas& c, const Theme& t)
{
    if (!active) return;
    int x, y;
    const int w = 200, h = 56;
    overlayPanel(c, t, w, h, x, y);
    c.setFont(&fonts::Font0);
    c.setTextSize(1);
    c.setTextDatum(textdatum_t::top_left);
    c.setTextColor(t.fg, t.panel);

    // naive wrap into two lines max
    String l1 = _text, l2 = "";
    int maxChars = (w - 16) / 6;
    if ((int)_text.length() > maxChars) {
        l1 = _text.substring(0, maxChars);
        l2 = _text.substring(maxChars, min((int)_text.length(), maxChars * 2));
    }
    c.drawString(l1, x + 8, y + 8);
    if (!l2.isEmpty()) c.drawString(l2, x + 8, y + 18);

    c.setTextColor(t.accent, t.panel);
    c.setTextDatum(textdatum_t::bottom_left);
    c.drawString(_mode == OK ? "Enter: OK" : "Y/Enter: yes   N/Esc: no", x + 8, y + h - 6);
}

}  // namespace ui
