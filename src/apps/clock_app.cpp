// ClaudeOS Clock — big clock (NTP-synced when WiFi is up) + uptime.
#include "clock_app.h"

#include <WiFi.h>
#include <time.h>

#include "../os/widgets.h"

namespace {

class ClockApp : public App {
public:
    const char* title() const override { return "Clock"; }

    void onKey(const KeyEvent& e) override
    {
        if (e.key == Key::Esc) OS::get().closeTop();
    }

    void onDraw(M5Canvas& c) override
    {
        const Theme& t = OS::get().theme();
        c.setFont(&fonts::Font0);
        c.setTextDatum(textdatum_t::middle_center);

        if (OS::get().timeSynced()) {
            time_t now = time(nullptr);
            struct tm tmv;
            localtime_r(&now, &tmv);
            char big[16], date[32];
            snprintf(big, sizeof(big), "%02d:%02d:%02d", tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
            static const char* WD[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
            snprintf(date, sizeof(date), "%04d-%02d-%02d (%s)", tmv.tm_year + 1900,
                     tmv.tm_mon + 1, tmv.tm_mday, WD[tmv.tm_wday]);

            c.setTextSize(3);
            c.setTextColor(t.accent, t.bg);
            c.drawString(big, SCREEN_W / 2, 58);
            c.setTextSize(1);
            c.setTextColor(t.fg, t.bg);
            c.drawString(date, SCREEN_W / 2, 88);
            char tz[24];
            snprintf(tz, sizeof(tz), "UTC%+d", OS::get().settings.tzHours);
            c.setTextColor(t.dim, t.bg);
            c.drawString(tz, SCREEN_W / 2, 100);
        } else {
            uint32_t s = OS::get().uptimeMs() / 1000;
            char big[16];
            snprintf(big, sizeof(big), "%02u:%02u:%02u", (unsigned)(s / 3600),
                     (unsigned)(s / 60 % 60), (unsigned)(s % 60));
            c.setTextSize(3);
            c.setTextColor(t.accent2, t.bg);
            c.drawString(big, SCREEN_W / 2, 58);
            c.setTextSize(1);
            c.setTextColor(t.dim, t.bg);
            c.drawString("uptime  (connect WiFi for real time)", SCREEN_W / 2, 90);
        }

        ui::hintBar(c, t, "Esc: back");
    }
};

}  // namespace

App* createClockApp()
{
    return new ClockApp();
}
