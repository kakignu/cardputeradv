// ClaudeOS Snake — the obligatory game.
#include "snake.h"

#include <deque>

#include "../os/widgets.h"

namespace {

constexpr int CELL   = 6;
constexpr int GRID_W = 38;
constexpr int GRID_H = 17;
constexpr int OFF_X  = (SCREEN_W - GRID_W * CELL) / 2;
constexpr int OFF_Y  = CONTENT_Y + 2;

struct P {
    int8_t x, y;
    bool operator==(const P& o) const { return x == o.x && y == o.y; }
};

class SnakeApp : public App {
public:
    const char* title() const override { return _titleBuf; }

    void onStart() override { reset(); }

    void onKey(const KeyEvent& e) override
    {
        switch (e.key) {
            case Key::Up:
                turn(0, -1);
                break;
            case Key::Down:
                turn(0, 1);
                break;
            case Key::Left:
                turn(-1, 0);
                break;
            case Key::Right:
                turn(1, 0);
                break;
            case Key::Enter:
                if (_dead) reset();
                break;
            case Key::Char:
                if (e.ch == ' ' && !_dead) _paused = !_paused;
                break;
            case Key::Esc:
                OS::get().closeTop();
                break;
            default:
                break;
        }
    }

    void onTick() override
    {
        if (_dead || _paused) return;
        uint32_t now = millis();
        if (now - _lastMove < _interval) return;
        _lastMove = now;

        _dir = _nextDir;
        P head = _body.front();
        head.x += _dir.x;
        head.y += _dir.y;
        if (head.x < 0 || head.y < 0 || head.x >= GRID_W || head.y >= GRID_H || hits(head)) {
            _dead = true;
            OS::get().jingle(false);
            return;
        }
        _body.push_front(head);
        if (head == _food) {
            _score++;
            snprintf(_titleBuf, sizeof(_titleBuf), "Snake  score %d", _score);
            if (_interval > 60) _interval -= 4;
            OS::get().beep(1046.5f + _score * 20, 25);
            placeFood();
        } else {
            _body.pop_back();
        }
    }

    void onDraw(M5Canvas& c) override
    {
        const Theme& t = OS::get().theme();
        c.drawRect(OFF_X - 2, OFF_Y - 2, GRID_W * CELL + 4, GRID_H * CELL + 4, t.panelHi);

        // food
        c.fillCircle(OFF_X + _food.x * CELL + CELL / 2, OFF_Y + _food.y * CELL + CELL / 2,
                     CELL / 2 - 1, t.danger);
        // snake
        bool head = true;
        for (const P& p : _body) {
            c.fillRoundRect(OFF_X + p.x * CELL, OFF_Y + p.y * CELL, CELL - 1, CELL - 1, 2,
                            head ? t.accent : t.ok);
            head = false;
        }

        if (_dead || _paused) {
            c.setFont(&fonts::Font0);
            c.setTextSize(2);
            c.setTextDatum(textdatum_t::middle_center);
            c.setTextColor(_dead ? t.danger : t.accent2, t.bg);
            c.drawString(_dead ? "GAME OVER" : "PAUSED", SCREEN_W / 2, SCREEN_H / 2 - 6);
            c.setTextSize(1);
            c.setTextColor(t.dim, t.bg);
            c.drawString(_dead ? "Enter: restart  Esc: quit" : "Space: resume", SCREEN_W / 2,
                         SCREEN_H / 2 + 12);
        }

        ui::hintBar(c, t, "Arrows: steer   Space: pause   Esc: quit");
    }

private:
    std::deque<P> _body;
    P _dir{1, 0}, _nextDir{1, 0}, _food{20, 8};
    bool _dead = false, _paused = false;
    int _score         = 0;
    uint32_t _interval = 140, _lastMove = 0;
    char _titleBuf[32] = "Snake";

    bool hits(const P& p)
    {
        for (const P& b : _body)
            if (b == p) return true;
        return false;
    }

    void turn(int dx, int dy)
    {
        if (_dir.x == -dx && _dir.y == -dy) return;  // no 180 turns
        _nextDir = {(int8_t)dx, (int8_t)dy};
    }

    void placeFood()
    {
        do {
            _food.x = random(GRID_W);
            _food.y = random(GRID_H);
        } while (hits(_food));
    }

    void reset()
    {
        _body.clear();
        _body.push_back({10, 8});
        _body.push_back({9, 8});
        _body.push_back({8, 8});
        _dir = _nextDir = {1, 0};
        _dead = _paused = false;
        _score    = 0;
        _interval = 140;
        snprintf(_titleBuf, sizeof(_titleBuf), "Snake");
        placeFood();
    }
};

}  // namespace

App* createSnakeApp()
{
    return new SnakeApp();
}
