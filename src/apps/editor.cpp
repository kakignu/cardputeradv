// ClaudeOS Edit — a small plain-text editor.
//
// Keys: type to insert, arrows to move (Ctrl+Left/Right = home/end,
// Ctrl+Up/Down = page), Ctrl+S save, Esc close (asks to save when dirty).
#include "editor.h"

#include <vector>

#include "../os/storage.h"
#include "../os/widgets.h"

namespace {

constexpr int MAX_LINES   = 500;
constexpr int MAX_LINELEN = 200;

class EditorApp : public App {
public:
    explicit EditorApp(const String& path) : _path(path) {}

    const char* title() const override { return _title.c_str(); }

    void onStart() override
    {
        if (!_path.isEmpty()) {
            String text;
            if (vfs::readText(_path, text)) {
                int start = 0;
                for (int i = 0; i <= (int)text.length(); i++) {
                    if (i == (int)text.length() || text[i] == '\n') {
                        if ((int)_lines.size() < MAX_LINES)
                            _lines.push_back(text.substring(start, i));
                        start = i + 1;
                    }
                }
            }
        }
        if (_lines.empty()) _lines.push_back("");
        updateTitle();
    }

    void onKey(const KeyEvent& e) override
    {
        if (_savePrompt.active) {
            int r = _savePrompt.handleKey(e);
            if (r == 1) {
                _path = _savePrompt.value;
                if (doSave() && _closeAfterSave) OS::get().closeTop();
            } else if (r == -1) {
                _closeAfterSave = false;
            }
            return;
        }
        if (_exitBox.active) {
            int r = _exitBox.handleKey(e);
            if (r == 1) {  // save
                _closeAfterSave = true;
                if (_path.isEmpty()) {
                    openSaveAs();
                } else if (doSave()) {
                    OS::get().closeTop();
                }
            } else if (r == -1) {  // discard
                OS::get().closeTop();
            }
            return;
        }
        if (_msg.active) {
            _msg.handleKey(e);
            return;
        }

        String& line = _lines[_cy];
        switch (e.key) {
            case Key::Char:
                if (e.ctrl && (e.ch == 's' || e.ch == 'S')) {
                    if (_path.isEmpty())
                        openSaveAs();
                    else
                        doSave();
                    break;
                }
                if (e.fn || e.ctrl) break;  // command layer — don't insert
                if ((int)line.length() < MAX_LINELEN) {
                    line = line.substring(0, _cx) + String(e.ch) + line.substring(_cx);
                    _cx++;
                    _dirty = true;
                }
                break;
            case Key::Tab:
                if ((int)line.length() < MAX_LINELEN - 2) {
                    line = line.substring(0, _cx) + "  " + line.substring(_cx);
                    _cx += 2;
                    _dirty = true;
                }
                break;
            case Key::Enter:
                if ((int)_lines.size() < MAX_LINES) {
                    String rest = line.substring(_cx);
                    line.remove(_cx);
                    _lines.insert(_lines.begin() + _cy + 1, rest);
                    _cy++;
                    _cx    = 0;
                    _dirty = true;
                }
                break;
            case Key::Backspace:
                if (_cx > 0) {
                    line.remove(_cx - 1, 1);
                    _cx--;
                    _dirty = true;
                } else if (_cy > 0) {
                    _cx = _lines[_cy - 1].length();
                    _lines[_cy - 1] += line;
                    _lines.erase(_lines.begin() + _cy);
                    _cy--;
                    _dirty = true;
                }
                break;
            case Key::Delete:
                if (_cx < (int)line.length()) {
                    line.remove(_cx, 1);
                    _dirty = true;
                } else if (_cy + 1 < (int)_lines.size()) {
                    line += _lines[_cy + 1];
                    _lines.erase(_lines.begin() + _cy + 1);
                    _dirty = true;
                }
                break;
            case Key::Up:
                if (e.ctrl)
                    _cy -= visRows();
                else
                    _cy--;
                if (_cy < 0) _cy = 0;
                clampX();
                break;
            case Key::Down:
                if (e.ctrl)
                    _cy += visRows();
                else
                    _cy++;
                if (_cy >= (int)_lines.size()) _cy = _lines.size() - 1;
                clampX();
                break;
            case Key::Left:
                if (e.ctrl) {
                    _cx = 0;
                } else if (_cx > 0) {
                    _cx--;
                } else if (_cy > 0) {
                    _cy--;
                    _cx = _lines[_cy].length();
                }
                break;
            case Key::Right:
                if (e.ctrl) {
                    _cx = line.length();
                } else if (_cx < (int)line.length()) {
                    _cx++;
                } else if (_cy + 1 < (int)_lines.size()) {
                    _cy++;
                    _cx = 0;
                }
                break;
            case Key::Esc:
                if (_dirty) {
                    _exitBox.open("Save changes?", ui::MsgBox::YESNO);
                } else {
                    OS::get().closeTop();
                }
                break;
            default:
                break;
        }
        updateTitle();
    }

    void onDraw(M5Canvas& c) override
    {
        const Theme& t = OS::get().theme();
        c.setFont(&fonts::Font0);
        c.setTextSize(1);
        c.setTextDatum(textdatum_t::top_left);

        int rows = visRows();
        if (_cy < _scrollY) _scrollY = _cy;
        if (_cy >= _scrollY + rows) _scrollY = _cy - rows + 1;
        int colOff = 0;
        if (_cx >= VIS_COLS) colOff = _cx - VIS_COLS + 1;

        for (int i = 0; i < rows; i++) {
            int idx = _scrollY + i;
            if (idx >= (int)_lines.size()) break;
            const String& ln = _lines[idx];
            String vis = (colOff < (int)ln.length())
                             ? ln.substring(colOff, min((int)ln.length(), colOff + VIS_COLS))
                             : "";
            int y = CONTENT_Y + i * 9;
            c.setTextColor(t.fg, t.bg);
            c.drawString(vis, 2, y);
            if (idx == _cy) {
                int cx  = 2 + (_cx - colOff) * 6;
                char ch = (_cx < (int)ln.length()) ? ln[_cx] : ' ';
                if ((millis() / 400) & 1) {
                    c.fillRect(cx, y - 1, 6, 9, t.accent);
                    c.setTextColor(t.selText, t.accent);
                    c.drawString(String(ch), cx, y);
                }
            }
        }

        char pos[96];
        snprintf(pos, sizeof(pos), "^S: save  Esc: close   Ln %d/%d Col %d", _cy + 1,
                 (int)_lines.size(), _cx + 1);
        ui::hintBar(c, t, pos);

        _savePrompt.draw(c, t);
        _exitBox.draw(c, t);
        _msg.draw(c, t);
    }

private:
    static constexpr int VIS_COLS = 39;
    static int visRows() { return (SCREEN_H - CONTENT_Y - 12) / 9; }

    String _path;
    String _title = "Edit";
    std::vector<String> _lines;
    int _cx = 0, _cy = 0, _scrollY = 0;
    bool _dirty = false, _closeAfterSave = false;
    ui::TextPrompt _savePrompt;
    ui::MsgBox _exitBox, _msg;

    void clampX()
    {
        if (_cx > (int)_lines[_cy].length()) _cx = _lines[_cy].length();
    }

    void updateTitle()
    {
        String name = _path.isEmpty() ? "(untitled)" : _path.substring(_path.lastIndexOf('/') + 1);
        _title      = "Edit " + name + (_dirty ? " *" : "");
    }

    void openSaveAs()
    {
        String def = OS::get().sdOk() ? "/sd/note.txt" : "/flash/note.txt";
        _savePrompt.open("Save as (full path):", def);
    }

    bool doSave()
    {
        String text;
        for (size_t i = 0; i < _lines.size(); i++) {
            text += _lines[i];
            if (i + 1 < _lines.size()) text += '\n';
        }
        if (vfs::writeText(_path, text)) {
            _dirty = false;
            OS::get().beep(1318.5f, 40);
            updateTitle();
            return true;
        }
        _msg.open("Save failed: " + _path, ui::MsgBox::OK);
        _closeAfterSave = false;
        return false;
    }
};

}  // namespace

App* createEditorApp(const String& path)
{
    return new EditorApp(path);
}
