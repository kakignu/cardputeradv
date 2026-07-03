// ClaudeOS Files — browse the microSD card and internal flash.
#include "files.h"

#include <vector>

#include "../os/storage.h"
#include "../os/widgets.h"
#include "editor.h"

namespace {

class FilesApp : public App {
public:
    const char* title() const override { return _title.c_str(); }

    void onStart() override
    {
        OS::get().remountSD();
        refresh();
    }

    void onResume() override { refresh(); }

    void onKey(const KeyEvent& e) override
    {
        if (_prompt.active) {
            int r = _prompt.handleKey(e);
            if (r == 1) onPromptDone();
            return;
        }
        if (_box.active) {
            int r = _box.handleKey(e);
            if (r == 1 && _boxAction == BOX_DELETE) doDelete();
            _boxAction = BOX_NONE;
            return;
        }

        if (_viewing) {
            viewerKey(e);
            return;
        }

        if (_lv.handleKey(e, listH())) return;

        switch (e.key) {
            case Key::Enter:
                openSelected();
                break;
            case Key::Backspace:
                goUp();
                break;
            case Key::Esc:
                if (vfs::isRoot(_cwd)) {
                    OS::get().closeTop();
                } else {
                    goUp();
                }
                break;
            case Key::Delete: {
                vfs::Entry* en = selected();
                if (en && !vfs::isRoot(_cwd)) {
                    _boxAction = BOX_DELETE;
                    _box.open("Delete '" + en->name + "' ?", ui::MsgBox::YESNO);
                }
                break;
            }
            case Key::Char:
                if (e.ch == 'n' && !vfs::isRoot(_cwd)) {
                    _promptAction = PROMPT_NEWDIR;
                    _prompt.open("New folder name:");
                } else if (e.ch == 'f' && !vfs::isRoot(_cwd)) {
                    _promptAction = PROMPT_NEWFILE;
                    _prompt.open("New file name:", "note.txt");
                } else if (e.ch == 'e') {
                    vfs::Entry* en = selected();
                    if (en && !en->dir) {
                        OS::get().launch(createEditorApp(vfs::join(_cwd, en->name)));
                    }
                } else if (e.ch == 'r') {
                    OS::get().remountSD();
                    refresh();
                }
                break;
            default:
                break;
        }
    }

    void onDraw(M5Canvas& c) override
    {
        const Theme& t = OS::get().theme();

        if (_viewing) {
            drawViewer(c, t);
            return;
        }

        if (_lv.items.empty()) {
            c.setFont(&fonts::Font0);
            c.setTextSize(1);
            c.setTextDatum(textdatum_t::middle_center);
            c.setTextColor(t.dim, t.bg);
            c.drawString(vfs::isRoot(_cwd) ? "No storage found (insert microSD, key R)"
                                           : "Empty folder",
                         SCREEN_W / 2, SCREEN_H / 2);
        } else {
            _lv.draw(c, t, 2, CONTENT_Y, SCREEN_W - 4, listH());
        }
        ui::hintBar(c, t,
                    "Enter: open  Bksp: up  E: edit  N/F: new  Fn+Bksp: del  Esc: back");
        _prompt.draw(c, t);
        _box.draw(c, t);
    }

private:
    enum PromptAction { PROMPT_NONE, PROMPT_NEWDIR, PROMPT_NEWFILE };
    enum BoxAction { BOX_NONE, BOX_DELETE };

    String _cwd = "/";
    String _title = "Files /";
    std::vector<vfs::Entry> _entries;
    ui::ListView _lv;
    ui::TextPrompt _prompt;
    ui::MsgBox _box;
    PromptAction _promptAction = PROMPT_NONE;
    BoxAction _boxAction       = BOX_NONE;

    // viewer state
    bool _viewing = false;
    std::vector<String> _viewLines;
    int _viewScroll = 0;
    String _viewName;

    static int listH() { return SCREEN_H - CONTENT_Y - 12; }

    vfs::Entry* selected()
    {
        if (_entries.empty() || _lv.sel < 0 || _lv.sel >= (int)_entries.size()) return nullptr;
        return &_entries[_lv.sel];
    }

    void refresh()
    {
        vfs::list(_cwd, _entries);
        _lv.items.clear();
        for (auto& e : _entries) {
            ui::ListView::Item it;
            it.text  = e.name;
            it.icon  = e.dir ? ui::ICON_FOLDER : ui::ICON_FILE;
            it.right = e.dir ? "" : ui::humanSize(e.size);
            _lv.items.push_back(it);
        }
        _lv.clampSel();
        _title = "Files " + _cwd;
        if (_title.length() > 32) _title = "Files ..." + _cwd.substring(_cwd.length() - 24);
    }

    void goUp()
    {
        if (vfs::isRoot(_cwd)) return;
        _cwd = vfs::parent(_cwd);
        _lv.sel = 0;
        refresh();
    }

    void openSelected()
    {
        vfs::Entry* en = selected();
        if (!en) return;
        String p = vfs::isRoot(_cwd) ? ("/" + en->name) : vfs::join(_cwd, en->name);
        if (en->dir) {
            _cwd    = p;
            _lv.sel = 0;
            refresh();
        } else {
            openViewer(p, en->name);
        }
    }

    void onPromptDone()
    {
        String name = _prompt.value;
        name.trim();
        if (name.isEmpty()) return;
        String p = vfs::join(_cwd, name);
        if (_promptAction == PROMPT_NEWDIR) {
            vfs::makeDir(p);
            refresh();
        } else if (_promptAction == PROMPT_NEWFILE) {
            vfs::touch(p);
            refresh();
            OS::get().launch(createEditorApp(p));
        }
        _promptAction = PROMPT_NONE;
    }

    void doDelete()
    {
        vfs::Entry* en = selected();
        if (!en) return;
        if (!vfs::removePath(vfs::join(_cwd, en->name))) {
            _box.open("Delete failed (folder not empty?)", ui::MsgBox::OK);
        }
        refresh();
    }

    // --- text viewer -----------------------------------------------------

    void openViewer(const String& path, const String& name)
    {
        String text;
        if (!vfs::readText(path, text)) {
            _box.open("Cannot read file", ui::MsgBox::OK);
            return;
        }
        _viewLines.clear();
        // wrap to 40 columns
        int lineStart = 0;
        for (int i = 0; i <= (int)text.length(); i++) {
            bool nl = (i == (int)text.length()) || text[i] == '\n';
            if (nl || i - lineStart >= 40) {
                _viewLines.push_back(text.substring(lineStart, i));
                lineStart = nl ? i + 1 : i;
            }
        }
        if (vfs::fileSize(path) > 30 * 1024) _viewLines.push_back("... (truncated at 30K)");
        _viewName   = name;
        _viewScroll = 0;
        _viewing    = true;
        _title      = "View " + name;
    }

    void viewerKey(const KeyEvent& e)
    {
        int page = viewerRows() - 1;
        switch (e.key) {
            case Key::Up:
                _viewScroll -= e.ctrl ? page : 1;
                break;
            case Key::Down:
                _viewScroll += e.ctrl ? page : 1;
                break;
            case Key::Esc:
            case Key::Backspace:
                _viewing = false;
                refresh();
                return;
            default:
                return;
        }
        int maxScroll = (int)_viewLines.size() - viewerRows();
        if (maxScroll < 0) maxScroll = 0;
        if (_viewScroll < 0) _viewScroll = 0;
        if (_viewScroll > maxScroll) _viewScroll = maxScroll;
    }

    static int viewerRows() { return (SCREEN_H - CONTENT_Y - 12) / 9; }

    void drawViewer(M5Canvas& c, const Theme& t)
    {
        c.setFont(&fonts::Font0);
        c.setTextSize(1);
        c.setTextDatum(textdatum_t::top_left);
        c.setTextColor(t.fg, t.bg);
        int rows = viewerRows();
        for (int i = 0; i < rows; i++) {
            int idx = _viewScroll + i;
            if (idx >= (int)_viewLines.size()) break;
            c.drawString(_viewLines[idx], 3, CONTENT_Y + i * 9);
        }
        char pos[32];
        snprintf(pos, sizeof(pos), "%d/%d", _viewScroll + 1, (int)_viewLines.size());
        String hint = String("Up/Down: scroll (") + pos + ")   Esc: back";
        ui::hintBar(c, t, hint.c_str());
    }
};

}  // namespace

App* createFilesApp()
{
    return new FilesApp();
}
