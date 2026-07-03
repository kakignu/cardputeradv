#include "keys.h"
#include <M5Cardputer.h>
#include <algorithm>

static void fillMods(KeyEvent& e, const bool ctrl, const bool shift, const bool alt,
                     const bool opt, const bool fn)
{
    e.ctrl  = ctrl;
    e.shift = shift;
    e.alt   = alt;
    e.opt   = opt;
    e.fn    = fn;
}

void KeyService::emitEvent(KeyEvent e, bool fresh)
{
    e.repeat = !fresh;
    _q.push_back(e);
    if (fresh) {
        _held      = e;
        _holding   = true;
        _holdSince = millis();
        _lastRepeat = _holdSince;
        if (onKeyFeedback) onKeyFeedback();
    } else {
        _lastRepeat = millis();
    }
}

bool KeyService::repeatable(Key k)
{
    switch (k) {
        case Key::Char:
        case Key::Backspace:
        case Key::Delete:
        case Key::Up:
        case Key::Down:
        case Key::Left:
        case Key::Right:
            return true;
        default:
            return false;
    }
}

bool KeyService::stillHeld(const Snap& s, const KeyEvent& e)
{
    switch (e.key) {
        case Key::Char:
            return std::find(s.chars.begin(), s.chars.end(), e.ch) != s.chars.end();
        case Key::Enter:
            return s.enter;
        case Key::Backspace:
            return s.backspace;
        case Key::Delete:
            return s.del;
        case Key::Esc:
            return s.esc;
        case Key::Tab:
            return s.tab;
        case Key::Up:
            return s.up;
        case Key::Down:
            return s.down;
        case Key::Left:
            return s.left;
        case Key::Right:
            return s.right;
        default:
            return false;
    }
}

void KeyService::poll()
{
    auto& ks = M5Cardputer.Keyboard.keysState();

    // Decode the M5Cardputer 1.1.x KeysState: `del` is the physical Backspace
    // key, and the fn layer (arrows, Esc, Delete) is not decoded by the
    // library — with fn held, `word` still carries the base characters.
    Snap cur;
    cur.enter = ks.enter;
    cur.tab   = ks.tab;
    cur.ctrl  = ks.ctrl;
    cur.shift = ks.shift;
    cur.alt   = ks.alt;
    cur.opt   = ks.opt;
    cur.fn    = ks.fn;
    cur.backspace = ks.del && !ks.fn;
    cur.del       = ks.del && ks.fn;
    for (char c : ks.word) {
        if (ks.fn) {
            // fn layer: `=Esc  ;=Up  .=Down  ,=Left  /=Right
            switch (c) {
                case '`': case '~': cur.esc = true; break;
                case ';': case ':': cur.up = true; break;
                case '.': case '>': cur.down = true; break;
                case ',': case '<': cur.left = true; break;
                case '/': case '?': cur.right = true; break;
                default: break;  // other keys do nothing on the fn layer
            }
        } else {
            cur.chars.push_back(c);
        }
    }
    if (!ks.fn && ks.space &&
        std::find(cur.chars.begin(), cur.chars.end(), ' ') == cur.chars.end()) {
        cur.chars.push_back(' ');
    }

    bool emitted = false;
    auto mk = [&](Key k, char c = 0) {
        KeyEvent e;
        e.key = k;
        // with Ctrl held the library reports the shifted glyph; normalize
        // letters back to lowercase so apps can test e.ctrl && e.ch=='s'
        if (k == Key::Char && cur.ctrl && !cur.shift && c >= 'A' && c <= 'Z') c += 32;
        e.ch = c;
        fillMods(e, cur.ctrl, cur.shift, cur.alt, cur.opt, cur.fn);
        emitEvent(e, true);
        emitted = true;
    };

    // Edge detection on special keys
    if (cur.enter && !_prev.enter) mk(Key::Enter);
    if (cur.backspace && !_prev.backspace) mk(Key::Backspace);
    if (cur.del && !_prev.del) mk(Key::Delete);
    if (cur.esc && !_prev.esc) mk(Key::Esc);
    if (cur.tab && !_prev.tab) mk(Key::Tab);
    if (cur.up && !_prev.up) mk(Key::Up);
    if (cur.down && !_prev.down) mk(Key::Down);
    if (cur.left && !_prev.left) mk(Key::Left);
    if (cur.right && !_prev.right) mk(Key::Right);

    // Edge detection on printable characters (multiset difference cur - prev)
    {
        std::vector<char> prevChars = _prev.chars;
        for (char c : cur.chars) {
            auto it = std::find(prevChars.begin(), prevChars.end(), c);
            if (it != prevChars.end()) {
                prevChars.erase(it);  // already held last frame
            } else {
                mk(Key::Char, c);
            }
        }
    }

    // Auto-repeat for the most recent held key
    if (!emitted && _holding) {
        if (!cur.anyNonModifier() || !stillHeld(cur, _held)) {
            _holding = false;
        } else if (repeatable(_held.key)) {
            uint32_t now = millis();
            if (now - _holdSince >= repeatDelayMs && now - _lastRepeat >= repeatRateMs) {
                KeyEvent e = _held;
                fillMods(e, cur.ctrl, cur.shift, cur.alt, cur.opt, cur.fn);
                emitEvent(e, false);
            }
        }
    }

    _prev = cur;
}

bool KeyService::next(KeyEvent& out)
{
    if (_q.empty()) return false;
    out = _q.front();
    _q.erase(_q.begin());
    return true;
}

void KeyService::clear()
{
    _q.clear();
    _holding = false;
}
