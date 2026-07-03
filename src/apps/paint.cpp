// ClaudeOS Paint — pixel doodles on an 8-bit canvas.
#include "paint.h"

#include "../os/storage.h"
#include "../os/widgets.h"

namespace {

constexpr int AREA_X = 8, AREA_Y = CONTENT_Y + 2;
constexpr int AREA_W = 224, AREA_H = 100;

const uint16_t PALETTE[10] = {
    C565(0xE8, 0xE4, 0xDC),  // 1 white
    C565(0xD9, 0x77, 0x57),  // 2 claude orange
    C565(0xE0, 0x5C, 0x4B),  // 3 red
    C565(0xF0, 0xC0, 0x40),  // 4 yellow
    C565(0x7B, 0xC4, 0x7F),  // 5 green
    C565(0x6C, 0xA0, 0xDC),  // 6 blue
    C565(0xB0, 0x80, 0xE0),  // 7 purple
    C565(0x8A, 0x94, 0xA0),  // 8 gray
    C565(0x50, 0x34, 0x28),  // 9 brown
    C565(0x00, 0x00, 0x00),  // 0 black (eraser on dark bg)
};

class PaintApp : public App {
public:
    const char* title() const override { return "Paint"; }

    void onStart() override
    {
        _spr = new M5Canvas(&M5Cardputer.Display);
        _spr->setColorDepth(8);
        _spr->createSprite(AREA_W, AREA_H);
        _spr->fillSprite(TFT_BLACK);
        _x = AREA_W / 2;
        _y = AREA_H / 2;
    }

    void onStop() override
    {
        if (_spr) {
            _spr->deleteSprite();
            delete _spr;
            _spr = nullptr;
        }
    }

    void onKey(const KeyEvent& e) override
    {
        if (_msg.active) {
            _msg.handleKey(e);
            return;
        }
        int step = e.ctrl ? 6 : 2;
        switch (e.key) {
            case Key::Up:
                move(0, -step);
                break;
            case Key::Down:
                move(0, step);
                break;
            case Key::Left:
                move(-step, 0);
                break;
            case Key::Right:
                move(step, 0);
                break;
            case Key::Char:
                if (e.ch >= '0' && e.ch <= '9') {
                    _color = (e.ch == '0') ? 9 : (e.ch - '1');
                } else if (e.ch == ' ') {
                    stamp();
                } else if (e.ch == 'p') {
                    _pen = !_pen;
                    if (_pen) stamp();
                } else if (e.ch == 'c') {
                    _spr->fillSprite(TFT_BLACK);
                } else if (e.ch == '-' && _size > 1) {
                    _size--;
                } else if (e.ch == '=' && _size < 6) {
                    _size++;
                } else if (e.ch == 's') {
                    saveBmp();
                }
                break;
            case Key::Esc:
                OS::get().closeTop();
                break;
            default:
                break;
        }
    }

    void onDraw(M5Canvas& c) override
    {
        const Theme& t = OS::get().theme();
        c.drawRect(AREA_X - 1, AREA_Y - 1, AREA_W + 2, AREA_H + 2, t.panelHi);
        if (_spr) _spr->pushSprite(&c, AREA_X, AREA_Y);

        // crosshair cursor
        int sx = AREA_X + _x, sy = AREA_Y + _y;
        uint16_t cc = ((millis() / 250) & 1) ? t.fg : PALETTE[_color];
        c.drawFastHLine(sx - 4, sy, 3, cc);
        c.drawFastHLine(sx + 2, sy, 3, cc);
        c.drawFastVLine(sx, sy - 4, 3, cc);
        c.drawFastVLine(sx, sy + 2, 3, cc);

        // palette bar in hint area
        char hint[64];
        snprintf(hint, sizeof(hint), "%s  sz%d  Spc:dot P:pen C:clr S:save", _pen ? "PEN" : "   ",
                 _size);
        ui::hintBar(c, t, hint);
        for (int i = 0; i < 10; i++) {
            int px = 150 + i * 8;
            c.fillRect(px, SCREEN_H - 9, 7, 8, PALETTE[i]);
            if (i == _color) c.drawRect(px - 1, SCREEN_H - 10, 9, 10, t.fg);
        }
        _msg.draw(c, t);
    }

private:
    M5Canvas* _spr = nullptr;
    int _x = 0, _y = 0, _color = 1, _size = 2;
    bool _pen = false;
    ui::MsgBox _msg;

    void move(int dx, int dy)
    {
        _x = constrain(_x + dx, 0, AREA_W - 1);
        _y = constrain(_y + dy, 0, AREA_H - 1);
        if (_pen) stamp();
    }

    void stamp()
    {
        if (_spr) _spr->fillCircle(_x, _y, _size - 1, PALETTE[_color]);
    }

    void saveBmp()
    {
        String root = OS::get().sdOk() ? "/sd" : (OS::get().flashOk() ? "/flash" : "");
        if (root.isEmpty()) {
            _msg.open("No storage for saving", ui::MsgBox::OK);
            return;
        }
        String path;
        for (int i = 1; i < 100; i++) {
            path = root + "/paint-" + String(i) + ".bmp";
            if (!vfs::exists(path)) break;
        }
        if (writeBmp(path)) {
            OS::get().beep(1318.5f, 50);
            _msg.open("Saved " + path, ui::MsgBox::OK);
        } else {
            _msg.open("Save failed", ui::MsgBox::OK);
        }
    }

    bool writeBmp(const String& path)
    {
        String sub;
        fs::FS* fs = vfs::resolve(path, sub);
        if (!fs || !_spr) return false;
        File f = fs->open(sub, FILE_WRITE);
        if (!f) return false;

        const int w = AREA_W, h = AREA_H;
        const int rowBytes = (w * 3 + 3) & ~3;
        const uint32_t dataSize = rowBytes * h;
        const uint32_t fileSize = 54 + dataSize;

        uint8_t hdr[54] = {0};
        hdr[0] = 'B'; hdr[1] = 'M';
        hdr[2] = fileSize & 0xFF; hdr[3] = (fileSize >> 8) & 0xFF;
        hdr[4] = (fileSize >> 16) & 0xFF; hdr[5] = (fileSize >> 24) & 0xFF;
        hdr[10] = 54;                    // pixel data offset
        hdr[14] = 40;                    // BITMAPINFOHEADER
        hdr[18] = w & 0xFF; hdr[19] = (w >> 8) & 0xFF;
        hdr[22] = h & 0xFF; hdr[23] = (h >> 8) & 0xFF;
        hdr[26] = 1;                     // planes
        hdr[28] = 24;                    // bpp
        hdr[34] = dataSize & 0xFF; hdr[35] = (dataSize >> 8) & 0xFF;
        hdr[36] = (dataSize >> 16) & 0xFF; hdr[37] = (dataSize >> 24) & 0xFF;
        f.write(hdr, 54);

        std::unique_ptr<uint8_t[]> row(new uint8_t[rowBytes]);
        for (int y = h - 1; y >= 0; y--) {
            memset(row.get(), 0, rowBytes);
            for (int x = 0; x < w; x++) {
                uint16_t p     = _spr->readPixel(x, y);
                row[x * 3 + 0] = (p & 0x1F) << 3;          // B
                row[x * 3 + 1] = ((p >> 5) & 0x3F) << 2;   // G
                row[x * 3 + 2] = ((p >> 11) & 0x1F) << 3;  // R
            }
            f.write(row.get(), rowBytes);
        }
        f.close();
        return true;
    }
};

}  // namespace

App* createPaintApp()
{
    return new PaintApp();
}
