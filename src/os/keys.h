// ClaudeOS — keyboard event service.
//
// Turns the raw "currently held keys" snapshot provided by the Cardputer ADV
// TCA8418 scanner (via M5Cardputer.Keyboard) into edge-triggered key events
// with auto-repeat, which is what applications actually want.
#pragma once
#include <Arduino.h>
#include <functional>
#include <vector>

enum class Key : uint8_t {
    None,
    Char,  // printable character in KeyEvent::ch
    Enter,
    Backspace,
    Delete,  // fn + backspace
    Esc,     // fn + `
    Tab,
    Up,     // fn + ;
    Down,   // fn + .
    Left,   // fn + ,
    Right,  // fn + /
};

struct KeyEvent {
    Key key   = Key::None;
    char ch   = 0;
    bool ctrl = false, shift = false, alt = false, opt = false, fn = false;
    bool repeat = false;
};

class KeyService {
public:
    // Call once per frame, after M5Cardputer.update().
    void poll();
    // Pop the next pending event. Returns false when the queue is empty.
    bool next(KeyEvent& out);
    void clear();

    // Called whenever a (non-repeat) event is generated — used for key click.
    std::function<void()> onKeyFeedback;

    uint16_t repeatDelayMs = 450;
    uint16_t repeatRateMs  = 60;

private:
    struct Snap {
        bool enter = false, backspace = false, del = false, esc = false, tab = false;
        bool up = false, down = false, left = false, right = false;
        bool ctrl = false, shift = false, alt = false, opt = false, fn = false;
        std::vector<char> chars;
        bool anyNonModifier() const
        {
            return enter || backspace || del || esc || tab || up || down || left || right ||
                   !chars.empty();
        }
    };

    Snap _prev;
    std::vector<KeyEvent> _q;
    KeyEvent _held;
    bool _holding      = false;
    uint32_t _holdSince = 0, _lastRepeat = 0;

    void emitEvent(KeyEvent e, bool fresh);
    static bool stillHeld(const Snap& s, const KeyEvent& e);
    static bool repeatable(Key k);
};
