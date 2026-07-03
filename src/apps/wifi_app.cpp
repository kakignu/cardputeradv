// ClaudeOS WiFi — scan, connect, remember credentials.
#include "wifi_app.h"

#include <Preferences.h>
#include <WiFi.h>
#include <vector>

#include "../os/widgets.h"

namespace {

class WifiApp : public App {
public:
    const char* title() const override { return "WiFi"; }

    void onStart() override
    {
        WiFi.mode(WIFI_STA);
        startScan();
    }

    void onKey(const KeyEvent& e) override
    {
        if (_pwPrompt.active) {
            int r = _pwPrompt.handleKey(e);
            if (r == 1) connect(_target, _pwPrompt.value);
            return;
        }
        if (_msg.active) {
            _msg.handleKey(e);
            return;
        }
        if (_lv.handleKey(e, listH())) return;

        switch (e.key) {
            case Key::Enter: {
                if (_scanning || _ssids.empty()) break;
                int i = _lv.sel;
                if (i < 0 || i >= (int)_ssids.size()) break;
                _target = _ssids[i];
                if (_open[i]) {
                    connect(_target, "");
                } else {
                    Preferences p;
                    p.begin("wifi", true);
                    String saved =
                        (p.getString("ssid", "") == _target) ? p.getString("pass", "") : "";
                    p.end();
                    _pwPrompt.open("Password for " + _target + ":", saved, true);
                }
                break;
            }
            case Key::Char:
                if (e.ch == 'r' && !_scanning) startScan();
                if (e.ch == 'd') {
                    WiFi.disconnect(true);
                    _msg.open("Disconnected", ui::MsgBox::OK);
                }
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
        if (_scanning) {
            int n = WiFi.scanComplete();
            if (n >= 0) {
                _scanning = false;
                _ssids.clear();
                _open.clear();
                _lv.items.clear();
                for (int i = 0; i < n; i++) {
                    String ssid = WiFi.SSID(i);
                    if (ssid.isEmpty()) continue;
                    bool dup = false;
                    for (auto& s : _ssids)
                        if (s == ssid) dup = true;
                    if (dup) continue;
                    _ssids.push_back(ssid);
                    bool open = WiFi.encryptionType(i) == WIFI_AUTH_OPEN;
                    _open.push_back(open);
                    ui::ListView::Item it;
                    it.text  = ssid;
                    it.icon  = ui::ICON_WIFI;
                    it.right = String(WiFi.RSSI(i)) + "dB" + (open ? " open" : "");
                    _lv.items.push_back(it);
                }
                WiFi.scanDelete();
            }
        }
        if (_connecting) {
            if (WiFi.status() == WL_CONNECTED) {
                _connecting = false;
                OS::get().jingle(true);
                // remember credentials
                Preferences p;
                p.begin("wifi", false);
                p.putString("ssid", _target);
                p.putString("pass", _lastPass);
                p.end();
                _msg.open("Connected! IP " + WiFi.localIP().toString(), ui::MsgBox::OK);
            } else if (millis() - _connectStart > 15000) {
                _connecting = false;
                OS::get().beep(220, 200);
                _msg.open("Connection failed", ui::MsgBox::OK);
                WiFi.disconnect();
            }
        }
    }

    void onDraw(M5Canvas& c) override
    {
        const Theme& t = OS::get().theme();

        // status header
        c.setFont(&fonts::Font0);
        c.setTextSize(1);
        c.setTextDatum(textdatum_t::top_left);
        String st;
        uint16_t stc;
        if (_connecting) {
            st  = "Connecting to " + _target + spinner();
            stc = t.accent;
        } else if (WiFi.status() == WL_CONNECTED) {
            st  = WiFi.SSID() + "  " + WiFi.localIP().toString();
            stc = t.ok;
        } else {
            st  = "Not connected";
            stc = t.dim;
        }
        c.setTextColor(stc, t.bg);
        c.drawString(st, 3, CONTENT_Y);

        int ly = CONTENT_Y + 11;
        if (_scanning) {
            c.setTextColor(t.dim, t.bg);
            c.drawString("Scanning" + spinner(), 3, ly + 4);
        } else if (_lv.items.empty()) {
            c.setTextColor(t.dim, t.bg);
            c.drawString("No networks found. R: rescan", 3, ly + 4);
        } else {
            _lv.draw(c, t, 2, ly, SCREEN_W - 4, listH());
        }

        ui::hintBar(c, t, "Enter: connect   R: rescan   D: disconnect   Esc: back");
        _pwPrompt.draw(c, t);
        _msg.draw(c, t);
    }

private:
    ui::ListView _lv;
    ui::TextPrompt _pwPrompt;
    ui::MsgBox _msg;
    std::vector<String> _ssids;
    std::vector<bool> _open;
    bool _scanning = false, _connecting = false;
    uint32_t _connectStart = 0;
    String _target, _lastPass;

    static int listH() { return SCREEN_H - (CONTENT_Y + 11) - 12; }

    String spinner()
    {
        const char sp[] = {'|', '/', '-', '\\'};
        return String(sp[(millis() / 150) % 4]);
    }

    void startScan()
    {
        _scanning = true;
        _lv.items.clear();
        _ssids.clear();
        _open.clear();
        WiFi.scanNetworks(true);
    }

    void connect(const String& ssid, const String& pass)
    {
        _lastPass = pass;
        WiFi.begin(ssid.c_str(), pass.c_str());
        _connecting   = true;
        _connectStart = millis();
    }
};

}  // namespace

App* createWifiApp()
{
    return new WifiApp();
}
