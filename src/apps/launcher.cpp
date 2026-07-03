// ClaudeOS launcher — home screen with an app grid.
#include "launcher.h"

#include "../os/registry.h"
#include "../os/widgets.h"

namespace {

constexpr int COLS   = 5;
constexpr int CELL_W = 46;
constexpr int CELL_H = 51;
constexpr int GRID_X = (SCREEN_W - COLS * CELL_W) / 2;
constexpr int GRID_Y = CONTENT_Y + 2;

class LauncherApp : public App {
public:
    const char* title() const override { return "ClaudeOS"; }

    void onKey(const KeyEvent& e) override
    {
        const auto& reg = appRegistry();
        int n           = reg.size();
        switch (e.key) {
            case Key::Left:
                _sel = (_sel + n - 1) % n;
                break;
            case Key::Right:
                _sel = (_sel + 1) % n;
                break;
            case Key::Up:
                if (_sel - COLS >= 0) _sel -= COLS;
                break;
            case Key::Down:
                if (_sel + COLS < n) _sel += COLS;
                break;
            case Key::Enter:
                OS::get().beep(880, 30);
                OS::get().launch(reg[_sel].create());
                break;
            case Key::Char: {
                // 1..9,0 quick launch
                if (e.ch >= '1' && e.ch <= '9' && (e.ch - '1') < n) {
                    _sel = e.ch - '1';
                    OS::get().launch(reg[_sel].create());
                } else if (e.ch == '0' && n >= 10) {
                    _sel = 9;
                    OS::get().launch(reg[_sel].create());
                }
                break;
            }
            default:
                break;
        }
    }

    void onDraw(M5Canvas& c) override
    {
        const Theme& t  = OS::get().theme();
        const auto& reg = appRegistry();

        for (int i = 0; i < (int)reg.size(); i++) {
            int col = i % COLS, row = i / COLS;
            int x = GRID_X + col * CELL_W;
            int y = GRID_Y + row * CELL_H;
            bool selected = (i == _sel);
            if (selected) {
                c.fillRoundRect(x + 1, y, CELL_W - 2, CELL_H - 2, 6, t.panelHi);
                c.drawRoundRect(x + 1, y, CELL_W - 2, CELL_H - 2, 6, t.accent);
            }
            reg[i].icon(c, x + (CELL_W - 24) / 2, y + 4, t);
            c.setFont(&fonts::Font0);
            c.setTextSize(1);
            c.setTextDatum(textdatum_t::top_center);
            c.setTextColor(selected ? t.fg : t.dim, selected ? t.panelHi : t.bg);
            c.drawString(reg[i].name, x + CELL_W / 2, y + 31);
            // shortcut number
            c.setTextDatum(textdatum_t::top_left);
            c.setTextColor(t.dim, selected ? t.panelHi : t.bg);
            char num = (i < 9) ? ('1' + i) : '0';
            c.drawString(String(num), x + 4, y + 2);
        }

        ui::hintBar(c, t, "Arrows: move   Enter: open   1-0: quick launch");
    }

private:
    int _sel = 0;
};

}  // namespace

App* createLauncherApp()
{
    return new LauncherApp();
}
