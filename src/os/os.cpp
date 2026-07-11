#include "os.h"

#include <LittleFS.h>
#include <Preferences.h>
#include <SD.h>
#include <SPI.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <time.h>

#include "../apps/launcher.h"
#include "proc.h"
#include "storage.h"

// one toast message travelling from a job thread to the UI thread
struct NotifyMsg {
    char text[56];
};

// SD card pins on Cardputer ADV (dedicated SPI bus, separate from the LCD)
static constexpr int PIN_SD_SCK  = 40;
static constexpr int PIN_SD_MISO = 39;
static constexpr int PIN_SD_MOSI = 14;
static constexpr int PIN_SD_CS   = 12;

static SPIClass s_sdSpi(HSPI);

OS& OS::get()
{
    static OS inst;
    return inst;
}

void OS::begin()
{
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);

    M5Cardputer.Display.setRotation(1);
    _canvas.setColorDepth(16);
    _canvas.createSprite(SCREEN_W, SCREEN_H);

    loadSettings();
    applySettings();

    vfs::initLocks();
    mountFlash();
    remountSD();

    proc::begin();
    _notifyQueue = xQueueCreate(8, sizeof(NotifyMsg));

    keys.onKeyFeedback = [this]() { click(); };

    _bootMillis = millis();
    splash();

    if (settings.wifiAuto) requestWifiAutoConnect();

    launch(createLauncherApp());
    applyPendingStackOps();
}

// --- settings ---------------------------------------------------------------

void OS::loadSettings()
{
    Preferences p;
    p.begin("claudeos", true);
    settings.brightness = p.getUChar("bright", settings.brightness);
    settings.volume     = p.getUChar("vol", settings.volume);
    settings.keyClick   = p.getBool("click", settings.keyClick);
    settings.bootSound  = p.getBool("bootsnd", settings.bootSound);
    settings.themeId    = p.getUChar("theme", settings.themeId);
    settings.tzHours    = p.getChar("tz", settings.tzHours);
    settings.wifiAuto   = p.getBool("wifiauto", settings.wifiAuto);
    settings.showFps    = p.getBool("fps", settings.showFps);
    p.end();
}

void OS::saveSettings()
{
    Preferences p;
    p.begin("claudeos", false);
    p.putUChar("bright", settings.brightness);
    p.putUChar("vol", settings.volume);
    p.putBool("click", settings.keyClick);
    p.putBool("bootsnd", settings.bootSound);
    p.putUChar("theme", settings.themeId);
    p.putChar("tz", settings.tzHours);
    p.putBool("wifiauto", settings.wifiAuto);
    p.putBool("fps", settings.showFps);
    p.end();
}

void OS::applySettings()
{
    if (settings.brightness < 10) settings.brightness = 10;
    M5Cardputer.Display.setBrightness(settings.brightness);
    M5Cardputer.Speaker.setVolume(settings.volume);
    _theme = settings.themeId == 1 ? lightTheme() : darkTheme();
}

// --- storage ----------------------------------------------------------------

void OS::mountFlash()
{
    _flashOk = LittleFS.begin(true);  // format on first use
}

bool OS::remountSD()
{
    if (_sdOk) return true;
    s_sdSpi.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
    _sdOk = SD.begin(PIN_SD_CS, s_sdSpi, 20000000);
    return _sdOk;
}

// --- sound ------------------------------------------------------------------

void OS::click()
{
    if (!settings.keyClick) return;
    M5Cardputer.Speaker.tone(4000, 8, 0);  // channel 0: UI sounds
}

void OS::beep(float freqHz, int ms)
{
    M5Cardputer.Speaker.tone(freqHz, ms, 0);  // channel 0: UI sounds
}

// --- notifications ------------------------------------------------------------

void OS::postNotify(const char* text)
{
    if (!_notifyQueue) return;
    NotifyMsg m;
    strncpy(m.text, text, sizeof(m.text) - 1);
    m.text[sizeof(m.text) - 1] = 0;
    xQueueSend((QueueHandle_t)_notifyQueue, &m, 0);  // drop when full
}

void OS::drainNotifications()
{
    if (!_notifyQueue) return;
    NotifyMsg m;
    while (xQueueReceive((QueueHandle_t)_notifyQueue, &m, 0) == pdTRUE) {
        Toast t;
        t.text  = m.text;
        t.until = millis() + 3000;
        _toasts.push_back(t);
        if (_toasts.size() > 3) _toasts.erase(_toasts.begin());
        beep(1567.98f, 45);
    }
    while (!_toasts.empty() && (int32_t)(millis() - _toasts.front().until) > 0) {
        _toasts.erase(_toasts.begin());
    }
}

void OS::drawToasts()
{
    if (_toasts.empty()) return;
    const Theme& t = _theme;
    _canvas.setFont(&fonts::Font0);
    _canvas.setTextSize(1);
    _canvas.setTextDatum(textdatum_t::middle_left);
    int y = STATUSBAR_H + 4;
    for (auto& toast : _toasts) {
        int w = _canvas.textWidth(toast.text) + 16;
        if (w > SCREEN_W - 8) w = SCREEN_W - 8;
        int x = SCREEN_W - w - 3;
        _canvas.fillRoundRect(x, y, w, 15, 4, t.panelHi);
        _canvas.drawRoundRect(x, y, w, 15, 4, t.accent);
        _canvas.setTextColor(t.fg, t.panelHi);
        _canvas.drawString(toast.text, x + 8, y + 8);
        y += 18;
    }
}

void OS::jingle(bool up)
{
    static const float upNotes[]   = {523.25f, 659.25f, 783.99f, 1046.5f};
    static const float downNotes[] = {783.99f, 659.25f, 523.25f, 392.0f};
    const float* notes = up ? upNotes : downNotes;
    for (int i = 0; i < 4; i++) {
        M5Cardputer.Speaker.tone(notes[i], 70);
        delay(75);
    }
}

// --- wifi / time ------------------------------------------------------------

void OS::requestWifiAutoConnect()
{
    Preferences p;
    p.begin("wifi", true);
    String ssid = p.getString("ssid", "");
    String pass = p.getString("pass", "");
    p.end();
    if (ssid.isEmpty()) return;
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());
}

void OS::pollTimeSync()
{
    if (_timeSynced) return;
    uint32_t now = millis();
    if (now - _lastTimePoll < 3000) return;
    _lastTimePoll = now;
    if (WiFi.status() != WL_CONNECTED) return;
    if (!_ntpStarted) {
        configTime(settings.tzHours * 3600L, 0, "pool.ntp.org", "time.google.com");
        _ntpStarted = true;
        return;
    }
    if (time(nullptr) > 1600000000) {  // sane epoch => sync done
        _timeSynced = true;
        beep(1046.5f, 40);
    }
}

// --- app stack ----------------------------------------------------------------

void OS::launch(App* app)
{
    if (app) _toLaunch.push_back(app);
}

void OS::closeTop()
{
    _toClose++;
}

void OS::applyPendingStackOps()
{
    while (_toClose > 0 && !_stack.empty()) {
        _toClose--;
        if (_stack.size() <= 1) continue;  // never close the launcher
        _stack.back()->onStop();
        _stack.pop_back();
        if (!_stack.empty() && _toLaunch.empty()) _stack.back()->onResume();
    }
    _toClose = 0;
    for (App* a : _toLaunch) {
        _stack.emplace_back(a);
        a->onStart();
    }
    _toLaunch.clear();
}

// --- boot splash ----------------------------------------------------------------

static void drawStarburst(M5Canvas& c, int cx, int cy, float r0, float r1, float phase,
                          uint16_t col)
{
    for (int i = 0; i < 8; i++) {
        float a  = phase + i * (PI / 4);
        int x0   = cx + (int)(cosf(a) * r0);
        int y0   = cy + (int)(sinf(a) * r0);
        int x1   = cx + (int)(cosf(a) * r1);
        int y1   = cy + (int)(sinf(a) * r1);
        c.drawLine(x0, y0, x1, y1, col);
        c.drawLine(x0 + 1, y0, x1 + 1, y1, col);
    }
}

void OS::splash()
{
    const uint32_t t0  = millis();
    const int frames   = 36;
    for (int f = 0; f <= frames; f++) {
        _canvas.fillSprite(_theme.bg);
        float k = f / (float)frames;
        drawStarburst(_canvas, SCREEN_W / 2, 52, 6, 6 + 16 * k, k * PI, _theme.accent);
        _canvas.setTextDatum(textdatum_t::middle_center);
        _canvas.setFont(&fonts::Font0);
        _canvas.setTextSize(2);
        _canvas.setTextColor(_theme.fg, _theme.bg);
        if (k > 0.4f) _canvas.drawString("ClaudeOS", SCREEN_W / 2, 92);
        _canvas.setTextSize(1);
        _canvas.setTextColor(_theme.dim, _theme.bg);
        if (k > 0.7f)
            _canvas.drawString("v" CLAUDEOS_VERSION "  for Cardputer ADV", SCREEN_W / 2, 112);
        _canvas.pushSprite(0, 0);
        if (settings.bootSound && f == 6) M5Cardputer.Speaker.tone(523.25f, 60);
        if (settings.bootSound && f == 14) M5Cardputer.Speaker.tone(783.99f, 60);
        if (settings.bootSound && f == 22) M5Cardputer.Speaker.tone(1046.5f, 90);
        delay(28);
    }
    (void)t0;
    delay(250);
}

// --- status bar ----------------------------------------------------------------

void OS::drawStatusBar()
{
    const Theme& t = _theme;
    _canvas.fillRect(0, 0, SCREEN_W, STATUSBAR_H, t.statusBg);
    _canvas.drawFastHLine(0, STATUSBAR_H, SCREEN_W, t.panelHi);

    _canvas.setFont(&fonts::Font0);
    _canvas.setTextSize(1);
    _canvas.setTextDatum(textdatum_t::middle_left);
    _canvas.setTextColor(t.accent, t.statusBg);

    App* a = top();
    String title = a ? a->title() : "ClaudeOS";
    _canvas.drawString(title, 4, STATUSBAR_H / 2);

    // background job badge
    int jobs = proc::runningCount();
    if (jobs > 0) {
        int bx = 6 + _canvas.textWidth(title);
        String badge = String(jobs) + "job";
        int bw = _canvas.textWidth(badge) + 8;
        _canvas.fillRoundRect(bx, 2, bw, 10, 3, t.panelHi);
        _canvas.setTextColor(t.accent2, t.panelHi);
        _canvas.drawString(badge, bx + 4, STATUSBAR_H / 2);
    }

    int rx = SCREEN_W - 4;

    // battery
    int lvl = M5Cardputer.Power.getBatteryLevel();
    if (lvl >= 0) {
        if (lvl > 100) lvl = 100;
        char buf[12];
        snprintf(buf, sizeof(buf), "%d%%", lvl);
        _canvas.setTextDatum(textdatum_t::middle_right);
        _canvas.setTextColor(lvl <= 20 ? t.danger : t.statusFg, t.statusBg);
        _canvas.drawString(buf, rx, STATUSBAR_H / 2);
        rx -= _canvas.textWidth(buf) + 4;
        // icon
        int bx = rx - 13;
        uint16_t bc = lvl <= 20 ? t.danger : t.statusFg;
        _canvas.drawRect(bx, 3, 12, 8, bc);
        _canvas.fillRect(bx + 12, 5, 2, 4, bc);
        int fill = (10 * lvl) / 100;
        if (fill > 0) _canvas.fillRect(bx + 1, 4, fill, 6, lvl <= 20 ? t.danger : t.ok);
        rx = bx - 5;
    }

    // wifi
    if (WiFi.status() == WL_CONNECTED) {
        int wx = rx - 9;
        _canvas.fillCircle(wx + 4, 10, 1, t.accent2);
        _canvas.drawArc(wx + 4, 10, 4, 4, 180, 360, t.accent2);
        _canvas.drawArc(wx + 4, 10, 7, 7, 180, 360, t.accent2);
        rx = wx - 5;
    }

    // sd
    if (_sdOk) {
        _canvas.setTextDatum(textdatum_t::middle_right);
        _canvas.setTextColor(t.dim, t.statusBg);
        _canvas.drawString("SD", rx, STATUSBAR_H / 2);
        rx -= _canvas.textWidth("SD") + 5;
    }

    // clock (centered) once NTP synced
    if (_timeSynced) {
        time_t now = time(nullptr);
        struct tm tmv;
        localtime_r(&now, &tmv);
        char buf[8];
        snprintf(buf, sizeof(buf), "%02d:%02d", tmv.tm_hour, tmv.tm_min);
        _canvas.setTextDatum(textdatum_t::middle_center);
        _canvas.setTextColor(t.statusFg, t.statusBg);
        _canvas.drawString(buf, SCREEN_W / 2, STATUSBAR_H / 2);
    }

    if (settings.showFps) {
        char buf[12];
        snprintf(buf, sizeof(buf), "%.0ffps", _fps);
        _canvas.setTextDatum(textdatum_t::middle_right);
        _canvas.setTextColor(t.dim, t.statusBg);
        _canvas.drawString(buf, rx, STATUSBAR_H / 2);
    }
}

// --- main loop ----------------------------------------------------------------

void OS::dispatchKeys()
{
    KeyEvent e;
    while (keys.next(e)) {
        // global hotkey: Ctrl+Alt+Del reboots
        if (e.ctrl && e.alt && e.key == Key::Delete) {
            ESP.restart();
        }
        App* a = top();
        if (a) a->onKey(e);
        applyPendingStackOps();
    }
}

void OS::tick()
{
    uint32_t frameStart = millis();

    M5Cardputer.update();
    keys.poll();
    dispatchKeys();

    App* a = top();
    if (a) a->onTick();
    applyPendingStackOps();
    pollTimeSync();
    drainNotifications();

    a = top();
    if (a) {
        _canvas.fillSprite(_theme.bg);
        a->onDraw(_canvas);
        if (!a->fullscreen()) drawStatusBar();
        drawToasts();
        _canvas.pushSprite(0, 0);
    }

    // ~30 fps frame pacing
    uint32_t elapsed = millis() - frameStart;
    _uiLoad = _uiLoad * 0.9f + (elapsed / 33.0f) * 0.1f;
    if (_uiLoad > 1.0f) _uiLoad = 1.0f;
    if (elapsed < 33) delay(33 - elapsed);
    uint32_t frameTime = millis() - _lastFrameMs;
    if (frameTime > 0) _fps = _fps * 0.9f + (1000.0f / frameTime) * 0.1f;
    _lastFrameMs = millis();
}
