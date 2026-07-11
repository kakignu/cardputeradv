#include "registry.h"

#include "../apps/clock_app.h"
#include "../apps/editor.h"
#include "../apps/files.h"
#include "../apps/imu_app.h"
#include "../apps/music.h"
#include "../apps/paint.h"
#include "../apps/tasks_app.h"
#include "../apps/settings_app.h"
#include "../apps/snake.h"
#include "../apps/sysinfo.h"
#include "../apps/terminal.h"
#include "../apps/wifi_app.h"

// ---- 24x24 vector icons ------------------------------------------------------

static void iconFiles(M5Canvas& c, int x, int y, const Theme& t)
{
    c.fillRoundRect(x + 2, y + 5, 9, 4, 2, t.accent);
    c.fillRoundRect(x + 2, y + 7, 20, 13, 2, t.accent);
    c.drawFastHLine(x + 4, y + 10, 16, t.selText);
}

static void iconEditor(M5Canvas& c, int x, int y, const Theme& t)
{
    c.drawRoundRect(x + 3, y + 2, 14, 20, 2, t.dim);
    c.drawFastHLine(x + 6, y + 6, 8, t.dim);
    c.drawFastHLine(x + 6, y + 10, 8, t.dim);
    c.drawFastHLine(x + 6, y + 14, 5, t.dim);
    // pencil
    c.drawLine(x + 14, y + 18, x + 21, y + 11, t.accent);
    c.drawLine(x + 15, y + 19, x + 22, y + 12, t.accent);
    c.fillTriangle(x + 13, y + 21, x + 14, y + 18, x + 16, y + 20, t.accent);
}

static void iconTerminal(M5Canvas& c, int x, int y, const Theme& t)
{
    c.fillRoundRect(x + 1, y + 3, 22, 18, 3, C565(0x10, 0x10, 0x14));
    c.drawRoundRect(x + 1, y + 3, 22, 18, 3, t.dim);
    c.setFont(&fonts::Font0);
    c.setTextSize(1);
    c.setTextDatum(textdatum_t::top_left);
    c.setTextColor(t.ok, C565(0x10, 0x10, 0x14));
    c.drawString(">_", x + 5, y + 8);
}

static void iconWifi(M5Canvas& c, int x, int y, const Theme& t)
{
    c.fillCircle(x + 12, y + 18, 2, t.accent2);
    c.drawArc(x + 12, y + 18, 7, 6, 180, 360, t.accent2);
    c.drawArc(x + 12, y + 18, 12, 11, 180, 360, t.accent2);
    c.drawArc(x + 12, y + 18, 17, 16, 180, 360, t.accent2);
}

static void iconClock(M5Canvas& c, int x, int y, const Theme& t)
{
    c.drawCircle(x + 12, y + 12, 10, t.accent);
    c.drawCircle(x + 12, y + 12, 9, t.accent);
    c.drawLine(x + 12, y + 12, x + 12, y + 6, t.fg);
    c.drawLine(x + 12, y + 12, x + 17, y + 14, t.fg);
    c.fillCircle(x + 12, y + 12, 1, t.fg);
}

static void iconLevel(M5Canvas& c, int x, int y, const Theme& t)
{
    c.drawCircle(x + 12, y + 12, 10, t.dim);
    c.drawFastHLine(x + 4, y + 12, 16, t.panelHi);
    c.drawFastVLine(x + 12, y + 4, 16, t.panelHi);
    c.fillCircle(x + 15, y + 9, 3, t.ok);
}

static void iconPaint(M5Canvas& c, int x, int y, const Theme& t)
{
    c.fillCircle(x + 11, y + 12, 9, t.panelHi);
    c.drawCircle(x + 11, y + 12, 9, t.dim);
    c.fillCircle(x + 8, y + 8, 2, t.danger);
    c.fillCircle(x + 14, y + 7, 2, t.accent2);
    c.fillCircle(x + 16, y + 13, 2, t.ok);
    c.fillCircle(x + 8, y + 15, 2, C565(0xF0, 0xC0, 0x40));
}

static void iconSnake(M5Canvas& c, int x, int y, const Theme& t)
{
    c.fillRoundRect(x + 3, y + 15, 6, 6, 2, t.ok);
    c.fillRoundRect(x + 8, y + 15, 6, 6, 2, t.ok);
    c.fillRoundRect(x + 13, y + 15, 6, 6, 2, t.ok);
    c.fillRoundRect(x + 13, y + 10, 6, 6, 2, t.ok);
    c.fillRoundRect(x + 13, y + 5, 6, 6, 2, t.accent);
    c.fillCircle(x + 6, y + 6, 2, t.danger);
}

static void iconSettings(M5Canvas& c, int x, int y, const Theme& t)
{
    int cx = x + 12, cy = y + 12;
    for (int i = 0; i < 8; i++) {
        float a = i * (PI / 4);
        c.fillCircle(cx + (int)(cosf(a) * 9), cy + (int)(sinf(a) * 9), 2, t.dim);
    }
    c.fillCircle(cx, cy, 7, t.dim);
    c.fillCircle(cx, cy, 3, t.bg);
}

static void iconMusic(M5Canvas& c, int x, int y, const Theme& t)
{
    c.fillEllipse(x + 6, y + 17, 4, 3, t.accent);
    c.fillRect(x + 9, y + 4, 2, 13, t.accent);
    c.fillEllipse(x + 16, y + 15, 4, 3, t.accent);
    c.fillRect(x + 19, y + 2, 2, 13, t.accent);
    c.fillRect(x + 9, y + 2, 12, 3, t.accent);
}

static void iconTasks(M5Canvas& c, int x, int y, const Theme& t)
{
    c.drawRoundRect(x + 1, y + 2, 22, 20, 3, t.dim);
    c.fillRect(x + 4, y + 6, 10, 3, t.ok);
    c.fillRect(x + 4, y + 11, 15, 3, t.accent2);
    c.fillRect(x + 4, y + 16, 7, 3, t.accent);
}

static void iconInfo(M5Canvas& c, int x, int y, const Theme& t)
{
    c.fillCircle(x + 12, y + 12, 10, t.accent2);
    c.setFont(&fonts::Font0);
    c.setTextSize(1);
    c.setTextDatum(textdatum_t::middle_center);
    c.setTextColor(t.selText, t.accent2);
    c.drawString("i", x + 12, y + 12);
}

// ---- registry -----------------------------------------------------------------

static App* createEditorDefault()
{
    return createEditorApp("");
}

const std::vector<AppInfo>& appRegistry()
{
    static const std::vector<AppInfo> reg = {
        {"files", "Files", iconFiles, createFilesApp},
        {"edit", "Edit", iconEditor, createEditorDefault},
        {"term", "Term", iconTerminal, createTerminalApp},
        {"wifi", "WiFi", iconWifi, createWifiApp},
        {"music", "Music", iconMusic, createMusicApp},
        {"tasks", "Tasks", iconTasks, createTasksApp},
        {"clock", "Clock", iconClock, createClockApp},
        {"level", "Level", iconLevel, createImuApp},
        {"paint", "Paint", iconPaint, createPaintApp},
        {"snake", "Snake", iconSnake, createSnakeApp},
        {"conf", "Config", iconSettings, createSettingsApp},
        {"info", "Info", iconInfo, createSysInfoApp},
    };
    return reg;
}

App* createAppById(const String& id)
{
    for (const auto& ai : appRegistry()) {
        if (id == ai.id) return ai.create();
    }
    return nullptr;
}
