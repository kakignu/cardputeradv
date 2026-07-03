// ClaudeOS Terminal — a tiny command shell.
#include "terminal.h"

#include <WiFi.h>
#include <deque>
#include <time.h>
#include <vector>

#include "../os/registry.h"
#include "../os/storage.h"
#include "../os/widgets.h"

namespace {

constexpr int COLS     = 40;
constexpr size_t MAXBUF = 250;

// color markers as first char of a stored line
constexpr char MK_ACCENT = '\x01';
constexpr char MK_ERROR  = '\x02';
constexpr char MK_OK     = '\x03';

const char* CMDS[] = {"help",  "about", "clear",   "uname",  "free",       "uptime", "battery",
                      "ls",    "cd",    "pwd",     "cat",    "rm",         "mkdir",  "touch",
                      "write", "scan",  "wifi",    "connect", "disconnect", "time",   "apps",
                      "open",  "bright", "vol",    "theme",  "sd",         "reboot", "exit"};

class TerminalApp : public App {
public:
    const char* title() const override { return "Terminal"; }

    void onStart() override
    {
        print(String(MK_ACCENT) + "ClaudeOS " CLAUDEOS_VERSION " shell");
        print("type 'help' for commands");
    }

    void onKey(const KeyEvent& e) override
    {
        switch (e.key) {
            case Key::Enter: {
                String cmd = _input;
                print("\x01" + _cwd + "> " + cmd);
                _input  = "";
                _cursor = 0;
                _scroll = 0;
                cmd.trim();
                if (!cmd.isEmpty()) {
                    _history.push_back(cmd);
                    if (_history.size() > 32) _history.erase(_history.begin());
                    _histPos = _history.size();
                    run(cmd);
                }
                break;
            }
            case Key::Backspace:
                if (_cursor > 0) {
                    _input.remove(_cursor - 1, 1);
                    _cursor--;
                }
                break;
            case Key::Delete:
                if (_cursor < (int)_input.length()) _input.remove(_cursor, 1);
                break;
            case Key::Tab: {  // simple command completion
                for (const char* c : CMDS) {
                    if (_input.length() > 0 && String(c).startsWith(_input)) {
                        _input  = String(c) + " ";
                        _cursor = _input.length();
                        break;
                    }
                }
                break;
            }
            case Key::Up:
                if (e.ctrl) {
                    _scroll += 4;
                    clampScroll();
                } else if (_histPos > 0) {
                    _histPos--;
                    _input  = _history[_histPos];
                    _cursor = _input.length();
                }
                break;
            case Key::Down:
                if (e.ctrl) {
                    _scroll -= 4;
                    clampScroll();
                } else if (_histPos < (int)_history.size()) {
                    _histPos++;
                    _input  = (_histPos == (int)_history.size()) ? "" : _history[_histPos];
                    _cursor = _input.length();
                }
                break;
            case Key::Left:
                if (_cursor > 0) _cursor--;
                break;
            case Key::Right:
                if (_cursor < (int)_input.length()) _cursor++;
                break;
            case Key::Esc:
                OS::get().closeTop();
                break;
            case Key::Char:
                if (e.ctrl && e.ch == 'c') {
                    _input  = "";
                    _cursor = 0;
                    break;
                }
                if (e.fn) break;
                if (_input.length() < 120) {
                    _input = _input.substring(0, _cursor) + String(e.ch) +
                             _input.substring(_cursor);
                    _cursor++;
                }
                break;
            default:
                break;
        }
    }

    void onTick() override
    {
        // async wifi scan results
        if (_scanning) {
            int n = WiFi.scanComplete();
            if (n >= 0) {
                _scanning = false;
                print(String(MK_OK) + String(n) + " network(s):");
                for (int i = 0; i < n && i < 20; i++) {
                    char buf[64];
                    snprintf(buf, sizeof(buf), " %-24s %4ddBm %s",
                             WiFi.SSID(i).substring(0, 24).c_str(), WiFi.RSSI(i),
                             WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "open" : "sec");
                    print(String(buf));
                }
                WiFi.scanDelete();
            }
        }
        // async connect feedback
        if (_connecting) {
            if (WiFi.status() == WL_CONNECTED) {
                _connecting = false;
                print(String(MK_OK) + "connected, IP " + WiFi.localIP().toString());
            } else if (millis() - _connectStart > 15000) {
                _connecting = false;
                print(String(MK_ERROR) + "connect timeout");
                WiFi.disconnect();
            }
        }
    }

    void onDraw(M5Canvas& c) override
    {
        const Theme& t = OS::get().theme();
        c.setFont(&fonts::Font0);
        c.setTextSize(1);
        c.setTextDatum(textdatum_t::top_left);

        const int rowH = 9;
        int rows       = (SCREEN_H - CONTENT_Y - rowH) / rowH;

        int total = _out.size();
        int first = total - rows - _scroll;
        if (first < 0) first = 0;
        for (int i = 0; i < rows; i++) {
            int idx = first + i;
            if (idx >= total) break;
            const String& raw = _out[idx];
            uint16_t col      = t.fg;
            const char* s     = raw.c_str();
            if (raw.length() && raw[0] == MK_ACCENT) {
                col = t.accent;
                s++;
            } else if (raw.length() && raw[0] == MK_ERROR) {
                col = t.danger;
                s++;
            } else if (raw.length() && raw[0] == MK_OK) {
                col = t.ok;
                s++;
            }
            c.setTextColor(col, t.bg);
            c.drawString(s, 2, CONTENT_Y + i * rowH);
        }

        // input line
        int iy = SCREEN_H - rowH;
        c.fillRect(0, iy - 1, SCREEN_W, rowH + 1, t.panel);
        String prompt = _cwd + "> ";
        String shown  = prompt + _input;
        int caret     = prompt.length() + _cursor;
        int start     = 0;
        if (caret >= COLS) start = caret - COLS + 1;
        c.setTextColor(t.fg, t.panel);
        c.drawString(shown.substring(start, min((int)shown.length(), start + COLS)), 2, iy);
        if ((millis() / 400) & 1) c.fillRect(2 + (caret - start) * 6, iy, 6, 8, t.accent);
    }

private:
    std::deque<String> _out;
    String _input, _cwd = "/";
    int _cursor = 0, _scroll = 0, _histPos = 0;
    std::vector<String> _history;
    bool _scanning = false, _connecting = false;
    uint32_t _connectStart = 0;

    void clampScroll()
    {
        int rows = (SCREEN_H - CONTENT_Y - 9) / 9;
        int maxS = (int)_out.size() - rows;
        if (maxS < 0) maxS = 0;
        if (_scroll < 0) _scroll = 0;
        if (_scroll > maxS) _scroll = maxS;
    }

    void print(const String& line)
    {
        // wrap, preserving a possible color marker
        char mark  = 0;
        String txt = line;
        if (txt.length() && (txt[0] == MK_ACCENT || txt[0] == MK_ERROR || txt[0] == MK_OK)) {
            mark = txt[0];
            txt  = txt.substring(1);
        }
        if (txt.isEmpty()) {
            _out.push_back("");
        }
        while (!txt.isEmpty()) {
            String chunk = txt.substring(0, COLS);
            _out.push_back(mark ? String(mark) + chunk : chunk);
            txt = txt.substring(chunk.length());
        }
        while (_out.size() > MAXBUF) _out.pop_front();
    }

    void err(const String& s) { print(String(MK_ERROR) + s); }
    void ok(const String& s) { print(String(MK_OK) + s); }

    static std::vector<String> tokenize(const String& cmd)
    {
        std::vector<String> tok;
        String cur;
        for (size_t i = 0; i < cmd.length(); i++) {
            if (cmd[i] == ' ') {
                if (!cur.isEmpty()) tok.push_back(cur);
                cur = "";
            } else {
                cur += cmd[i];
            }
        }
        if (!cur.isEmpty()) tok.push_back(cur);
        return tok;
    }

    String absPath(const String& p)
    {
        if (p.isEmpty()) return _cwd;
        if (p.startsWith("/")) return p;
        if (p == "..") return vfs::parent(_cwd);
        return vfs::join(_cwd, p);
    }

    void run(const String& cmdline)
    {
        auto tok = tokenize(cmdline);
        if (tok.empty()) return;
        String cmd = tok[0];
        cmd.toLowerCase();
        String a1 = tok.size() > 1 ? tok[1] : "";

        if (cmd == "help") {
            print("files : ls cd pwd cat rm mkdir touch write");
            print("net   : scan wifi connect disconnect");
            print("sys   : free uptime battery time bright vol");
            print("        theme sd uname about reboot");
            print("apps  : apps open <id>");
            print("misc  : clear exit  (Ctrl+Up/Dn scrolls)");
        } else if (cmd == "about") {
            print(String(MK_ACCENT) + "ClaudeOS " CLAUDEOS_VERSION);
            print("a tiny OS for Cardputer ADV,");
            print("written by Claude (Anthropic).");
        } else if (cmd == "uname") {
            print("ClaudeOS " CLAUDEOS_VERSION " esp32s3 " + String(ESP.getChipModel()));
        } else if (cmd == "clear") {
            _out.clear();
        } else if (cmd == "free") {
            print("heap  free " + ui::humanSize(ESP.getFreeHeap()) + " / " +
                  ui::humanSize(ESP.getHeapSize()));
            print("      min  " + ui::humanSize(ESP.getMinFreeHeap()));
        } else if (cmd == "uptime") {
            uint32_t s = OS::get().uptimeMs() / 1000;
            char buf[32];
            snprintf(buf, sizeof(buf), "%02u:%02u:%02u", (unsigned)(s / 3600),
                     (unsigned)(s / 60 % 60), (unsigned)(s % 60));
            print(buf);
        } else if (cmd == "battery") {
            print("battery " + String(M5Cardputer.Power.getBatteryLevel()) + "%");
        } else if (cmd == "time") {
            if (OS::get().timeSynced()) {
                time_t now = time(nullptr);
                struct tm tmv;
                localtime_r(&now, &tmv);
                char buf[40];
                strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmv);
                print(buf);
            } else {
                err("time not synced (connect wifi first)");
            }
        } else if (cmd == "ls") {
            String p = absPath(a1);
            std::vector<vfs::Entry> es;
            if (!vfs::list(p, es)) {
                err("ls: cannot open " + p);
            } else {
                for (auto& e : es)
                    print((e.dir ? " <dir>  " : "        ") + e.name +
                          (e.dir ? "" : "  " + ui::humanSize(e.size)));
                if (es.empty()) print("(empty)");
            }
        } else if (cmd == "cd") {
            String p = absPath(a1.isEmpty() ? "/" : a1);
            if (vfs::isDir(p))
                _cwd = p;
            else
                err("cd: not a directory: " + p);
        } else if (cmd == "pwd") {
            print(_cwd);
        } else if (cmd == "cat") {
            String text;
            if (a1.isEmpty())
                err("usage: cat <file>");
            else if (vfs::readText(absPath(a1), text, 4096)) {
                int start = 0;
                for (int i = 0; i <= (int)text.length(); i++) {
                    if (i == (int)text.length() || text[i] == '\n') {
                        print(text.substring(start, i));
                        start = i + 1;
                    }
                }
            } else
                err("cat: cannot read " + a1);
        } else if (cmd == "rm") {
            if (a1.isEmpty())
                err("usage: rm <path>");
            else if (vfs::removePath(absPath(a1)))
                ok("removed");
            else
                err("rm failed");
        } else if (cmd == "mkdir") {
            if (a1.isEmpty())
                err("usage: mkdir <dir>");
            else if (vfs::makeDir(absPath(a1)))
                ok("created");
            else
                err("mkdir failed");
        } else if (cmd == "touch") {
            if (a1.isEmpty())
                err("usage: touch <file>");
            else if (vfs::touch(absPath(a1)))
                ok("ok");
            else
                err("touch failed");
        } else if (cmd == "write") {
            if (tok.size() < 3) {
                err("usage: write <file> <text...>");
            } else {
                String text;
                for (size_t i = 2; i < tok.size(); i++) {
                    if (i > 2) text += ' ';
                    text += tok[i];
                }
                text += '\n';
                if (vfs::writeText(absPath(a1), text))
                    ok("wrote " + String(text.length()) + " bytes");
                else
                    err("write failed");
            }
        } else if (cmd == "scan") {
            WiFi.mode(WIFI_STA);
            WiFi.scanNetworks(true);
            _scanning = true;
            print("scanning...");
        } else if (cmd == "wifi") {
            if (WiFi.status() == WL_CONNECTED) {
                ok("connected to " + WiFi.SSID());
                print("IP " + WiFi.localIP().toString() + "  RSSI " + String(WiFi.RSSI()) +
                      "dBm");
                print("MAC " + WiFi.macAddress());
            } else {
                print("not connected (try: scan / connect)");
            }
        } else if (cmd == "connect") {
            if (tok.size() < 2) {
                err("usage: connect <ssid> [password]");
            } else {
                String pass = tok.size() > 2 ? tok[2] : "";
                WiFi.mode(WIFI_STA);
                WiFi.begin(tok[1].c_str(), pass.c_str());
                _connecting   = true;
                _connectStart = millis();
                print("connecting to " + tok[1] + "...");
            }
        } else if (cmd == "disconnect") {
            WiFi.disconnect(true);
            print("disconnected");
        } else if (cmd == "apps") {
            for (auto& ai : appRegistry()) print(String("  ") + ai.id + " - " + ai.name);
        } else if (cmd == "open") {
            App* a = createAppById(a1);
            if (a)
                OS::get().launch(a);
            else
                err("unknown app: " + a1 + " (see 'apps')");
        } else if (cmd == "bright") {
            int v = a1.toInt();
            if (v >= 10 && v <= 255) {
                OS::get().settings.brightness = v;
                OS::get().applySettings();
                OS::get().saveSettings();
                ok("brightness " + String(v));
            } else
                err("usage: bright <10-255>");
        } else if (cmd == "vol") {
            int v = a1.toInt();
            if (v >= 0 && v <= 255 && a1.length()) {
                OS::get().settings.volume = v;
                OS::get().applySettings();
                OS::get().saveSettings();
                OS::get().beep(880, 60);
                ok("volume " + String(v));
            } else
                err("usage: vol <0-255>");
        } else if (cmd == "theme") {
            if (a1 == "dark" || a1 == "light") {
                OS::get().settings.themeId = (a1 == "light") ? 1 : 0;
                OS::get().applySettings();
                OS::get().saveSettings();
                ok("theme: " + a1);
            } else
                err("usage: theme dark|light");
        } else if (cmd == "sd") {
            if (OS::get().remountSD())
                ok("sd mounted");
            else
                err("no sd card");
        } else if (cmd == "reboot") {
            ESP.restart();
        } else if (cmd == "exit") {
            OS::get().closeTop();
        } else {
            err("unknown command: " + cmd + " (try 'help')");
        }
    }
};

}  // namespace

App* createTerminalApp()
{
    return new TerminalApp();
}
