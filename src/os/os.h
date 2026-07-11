// ClaudeOS — kernel core.
//
// A small cooperative "operating system" for the M5Stack Cardputer ADV.
// Applications are stacked (launcher at the bottom); the top app receives
// key events and draws into a full-screen off-screen canvas at ~30 fps.
#pragma once
#include <M5Cardputer.h>
#include <memory>
#include <vector>

#include "keys.h"
#include "theme.h"

#ifndef CLAUDEOS_VERSION
#define CLAUDEOS_VERSION "0.0.0"
#endif

// Screen layout constants
constexpr int SCREEN_W    = 240;
constexpr int SCREEN_H    = 135;
constexpr int STATUSBAR_H = 14;
constexpr int CONTENT_Y   = STATUSBAR_H + 2;
constexpr int CONTENT_H   = SCREEN_H - CONTENT_Y;

class App {
public:
    virtual ~App() = default;
    virtual const char* title() const = 0;
    virtual void onStart() {}                  // app pushed on the stack
    virtual void onResume() {}                 // an app above closed
    virtual void onStop() {}                   // about to be destroyed
    virtual void onKey(const KeyEvent& e) = 0;
    virtual void onTick() {}                   // logic, once per frame
    virtual void onDraw(M5Canvas& c) = 0;      // draw content
    virtual bool fullscreen() const { return false; }  // hide status bar
};

struct SettingsData {
    uint8_t brightness = 180;   // 10..255
    uint8_t volume     = 96;    // 0..255
    bool keyClick      = true;
    bool bootSound     = true;
    uint8_t themeId    = 0;     // 0 = dark, 1 = light
    int8_t tzHours     = 9;     // UTC offset, default JST
    bool wifiAuto      = false; // auto-connect with saved credentials at boot
    bool showFps       = false;
};

class OS {
public:
    static OS& get();

    void begin();
    void tick();  // one frame: input -> logic -> draw

    M5Canvas& gfx() { return _canvas; }
    const Theme& theme() const { return _theme; }

    // --- app stack -------------------------------------------------------
    // Both are deferred until the current event handler returns, so an app
    // may safely call closeTop() from inside its own onKey().
    void launch(App* app);  // takes ownership
    void closeTop();
    App* top() { return _stack.empty() ? nullptr : _stack.back().get(); }

    // --- settings --------------------------------------------------------
    SettingsData settings;
    void loadSettings();
    void saveSettings();
    void applySettings();  // brightness / volume / theme

    // --- storage ---------------------------------------------------------
    bool sdOk() const { return _sdOk; }
    bool flashOk() const { return _flashOk; }
    bool remountSD();

    // --- sound -----------------------------------------------------------
    void click();
    void beep(float freqHz, int ms);
    void jingle(bool up);

    // --- notifications (thread-safe: callable from background jobs) -------
    void postNotify(const char* text);

    // --- misc ------------------------------------------------------------
    bool timeSynced() const { return _timeSynced; }
    uint32_t uptimeMs() const { return millis() - _bootMillis; }
    float fps() const { return _fps; }
    float uiLoad() const { return _uiLoad; }  // 0..1, busy fraction of the frame budget
    void requestWifiAutoConnect();

    KeyService keys;

private:
    OS() : _canvas(&M5Cardputer.Display) {}
    void splash();
    void drawStatusBar();
    void dispatchKeys();
    void applyPendingStackOps();
    void mountFlash();
    void pollTimeSync();
    void pollBattery();
    void drainNotifications();
    void drawToasts();

    Theme _theme = darkTheme();
    M5Canvas _canvas;
    std::vector<std::unique_ptr<App>> _stack;
    std::vector<App*> _toLaunch;
    int _toClose = 0;

    bool _sdOk = false, _flashOk = false;
    bool _timeSynced = false, _ntpStarted = false;
    uint32_t _bootMillis = 0, _lastFrameMs = 0, _lastTimePoll = 0;
    float _fps = 0, _uiLoad = 0;

    // battery: the raw ADC reading jitters by a few percent, so the status
    // bar shows a slow-polled, smoothed, hysteresis-filtered value
    uint32_t _lastBattPoll = 0;
    float _battEma  = -1;
    int _battShown  = -1;

    struct Toast {
        String text;
        uint32_t until;
    };
    void* _notifyQueue = nullptr;  // FreeRTOS queue (jobs -> UI thread)
    std::vector<Toast> _toasts;
};
