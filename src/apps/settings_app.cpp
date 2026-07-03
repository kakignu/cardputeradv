// ClaudeOS Settings — adjust and persist system preferences.
#include "settings_app.h"

#include "../os/widgets.h"

namespace {

enum RowId {
    ROW_BRIGHT,
    ROW_VOLUME,
    ROW_KEYCLICK,
    ROW_BOOTSND,
    ROW_THEME,
    ROW_TZ,
    ROW_WIFIAUTO,
    ROW_FPS,
    ROW_COUNT,
};

const char* ROW_NAMES[ROW_COUNT] = {
    "Brightness", "Volume",          "Key click",    "Boot sound",
    "Theme",      "Timezone (UTC+)", "WiFi auto-connect", "Show FPS",
};

class SettingsApp : public App {
public:
    const char* title() const override { return "Settings"; }

    void onKey(const KeyEvent& e) override
    {
        auto& s = OS::get().settings;
        switch (e.key) {
            case Key::Up:
                _sel = (_sel + ROW_COUNT - 1) % ROW_COUNT;
                break;
            case Key::Down:
                _sel = (_sel + 1) % ROW_COUNT;
                break;
            case Key::Left:
                adjust(-1);
                break;
            case Key::Right:
            case Key::Enter:
                adjust(1);
                break;
            case Key::Esc:
                OS::get().closeTop();
                break;
            default:
                return;
        }
        (void)s;
    }

    void onDraw(M5Canvas& c) override
    {
        const Theme& t = OS::get().theme();
        auto& s        = OS::get().settings;

        c.setFont(&fonts::Font0);
        c.setTextSize(1);
        const int rowH = 13;
        for (int i = 0; i < ROW_COUNT; i++) {
            int y = CONTENT_Y + 1 + i * rowH;
            bool selRow = (i == _sel);
            if (selRow) c.fillRoundRect(2, y, SCREEN_W - 4, rowH - 1, 3, t.sel);
            c.setTextDatum(textdatum_t::middle_left);
            c.setTextColor(selRow ? t.selText : t.fg, selRow ? t.sel : t.bg);
            c.drawString(ROW_NAMES[i], 8, y + rowH / 2);

            String v = valueText(i, s);
            c.setTextDatum(textdatum_t::middle_right);
            c.drawString((selRow ? "< " + v + " >" : v), SCREEN_W - 8, y + rowH / 2);
        }

        ui::hintBar(c, t, "Left/Right: change   Esc: back   (saved instantly)");
    }

private:
    int _sel = 0;

    static String valueText(int row, const SettingsData& s)
    {
        switch (row) {
            case ROW_BRIGHT:
                return String((s.brightness * 100) / 255) + "%";
            case ROW_VOLUME:
                return String((s.volume * 100) / 255) + "%";
            case ROW_KEYCLICK:
                return s.keyClick ? "on" : "off";
            case ROW_BOOTSND:
                return s.bootSound ? "on" : "off";
            case ROW_THEME:
                return s.themeId == 1 ? "light" : "dark";
            case ROW_TZ:
                return String((int)s.tzHours);
            case ROW_WIFIAUTO:
                return s.wifiAuto ? "on" : "off";
            case ROW_FPS:
                return s.showFps ? "on" : "off";
        }
        return "";
    }

    void adjust(int dir)
    {
        auto& os = OS::get();
        auto& s  = os.settings;
        switch (_sel) {
            case ROW_BRIGHT: {
                int v = s.brightness + dir * 25;
                s.brightness = constrain(v, 10, 255);
                break;
            }
            case ROW_VOLUME: {
                int v = s.volume + dir * 16;
                s.volume = constrain(v, 0, 255);
                break;
            }
            case ROW_KEYCLICK:
                s.keyClick = !s.keyClick;
                break;
            case ROW_BOOTSND:
                s.bootSound = !s.bootSound;
                break;
            case ROW_THEME:
                s.themeId = s.themeId == 1 ? 0 : 1;
                break;
            case ROW_TZ: {
                int v = s.tzHours + dir;
                if (v > 14) v = -12;
                if (v < -12) v = 14;
                s.tzHours = v;
                break;
            }
            case ROW_WIFIAUTO:
                s.wifiAuto = !s.wifiAuto;
                break;
            case ROW_FPS:
                s.showFps = !s.showFps;
                break;
        }
        os.applySettings();
        os.saveSettings();
        if (_sel == ROW_VOLUME) os.beep(880, 40);
    }
};

}  // namespace

App* createSettingsApp()
{
    return new SettingsApp();
}
