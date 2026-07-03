// ClaudeOS System Info.
#include "sysinfo.h"

#include <SD.h>
#include <WiFi.h>

#include "../os/widgets.h"

namespace {

class SysInfoApp : public App {
public:
    const char* title() const override { return "System Info"; }

    void onStart() override { refresh(); }

    void onKey(const KeyEvent& e) override
    {
        if (e.key == Key::Esc) OS::get().closeTop();
        if (e.key == Key::Char && e.ch == 'r') refresh();
        _lv.handleKey(e, SCREEN_H - CONTENT_Y - 12);
    }

    void onDraw(M5Canvas& c) override
    {
        const Theme& t = OS::get().theme();
        _lv.draw(c, t, 2, CONTENT_Y, SCREEN_W - 4, SCREEN_H - CONTENT_Y - 12);
        ui::hintBar(c, t, "R: refresh   Esc: back");
    }

private:
    ui::ListView _lv;

    void add(const String& k, const String& v)
    {
        ui::ListView::Item it;
        it.text  = k;
        it.right = v;
        _lv.items.push_back(it);
    }

    void refresh()
    {
        _lv.items.clear();
        _lv.rowH = 12;

        add("OS", "ClaudeOS " CLAUDEOS_VERSION);
        add("Chip", String(ESP.getChipModel()) + " x" + String(ESP.getChipCores()));
        add("CPU freq", String(ESP.getCpuFreqMHz()) + " MHz");
        add("Flash", ui::humanSize(ESP.getFlashChipSize()));
        add("Heap free", ui::humanSize(ESP.getFreeHeap()));
        add("Heap min", ui::humanSize(ESP.getMinFreeHeap()));
        add("SDK", ESP.getSdkVersion());

        int lvl = M5Cardputer.Power.getBatteryLevel();
        add("Battery", String(lvl) + "%");

        uint32_t s = OS::get().uptimeMs() / 1000;
        char up[24];
        snprintf(up, sizeof(up), "%02u:%02u:%02u", (unsigned)(s / 3600), (unsigned)(s / 60 % 60),
                 (unsigned)(s % 60));
        add("Uptime", up);

        if (OS::get().sdOk()) {
            add("microSD", ui::humanSize(SD.cardSize()));
        } else {
            add("microSD", "none");
        }
        add("WiFi MAC", WiFi.macAddress());
        if (WiFi.status() == WL_CONNECTED) {
            add("WiFi", WiFi.SSID());
            add("IP", WiFi.localIP().toString());
        } else {
            add("WiFi", "not connected");
        }
    }
};

}  // namespace

App* createSysInfoApp()
{
    return new SysInfoApp();
}
