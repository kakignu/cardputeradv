// ClaudeOS Level — bubble level + raw values from the BMI270 IMU.
#include "imu_app.h"

#include "../os/widgets.h"

namespace {

class ImuApp : public App {
public:
    const char* title() const override { return "Level (BMI270)"; }

    void onStart() override { _enabled = M5.Imu.isEnabled(); }

    void onKey(const KeyEvent& e) override
    {
        if (e.key == Key::Esc) OS::get().closeTop();
    }

    void onTick() override
    {
        if (!_enabled) return;
        M5.Imu.update();
        float ax, ay, az, gx, gy, gz;
        M5.Imu.getAccel(&ax, &ay, &az);
        M5.Imu.getGyro(&gx, &gy, &gz);
        // light smoothing
        _ax = _ax * 0.7f + ax * 0.3f;
        _ay = _ay * 0.7f + ay * 0.3f;
        _az = _az * 0.7f + az * 0.3f;
        _gx = gx;
        _gy = gy;
        _gz = gz;
    }

    void onDraw(M5Canvas& c) override
    {
        const Theme& t = OS::get().theme();
        if (!_enabled) {
            c.setFont(&fonts::Font0);
            c.setTextSize(1);
            c.setTextDatum(textdatum_t::middle_center);
            c.setTextColor(t.danger, t.bg);
            c.drawString("IMU not detected", SCREEN_W / 2, SCREEN_H / 2);
            ui::hintBar(c, t, "Esc: back");
            return;
        }

        const int cx = 62, cy = CONTENT_Y + (SCREEN_H - CONTENT_Y - 12) / 2, R = 46;
        // level rings
        c.drawCircle(cx, cy, R, t.panelHi);
        c.drawCircle(cx, cy, R / 2, t.panel);
        c.drawFastHLine(cx - R, cy, 2 * R, t.panel);
        c.drawFastVLine(cx, cy - R, 2 * R, t.panel);
        // bubble
        float bx = -_ax, by = _ay;
        if (bx < -1) bx = -1;
        if (bx > 1) bx = 1;
        if (by < -1) by = -1;
        if (by > 1) by = 1;
        int px = cx + (int)(bx * (R - 6));
        int py = cy + (int)(by * (R - 6));
        bool centered = (bx * bx + by * by) < 0.004f;
        c.fillCircle(px, py, 5, centered ? t.ok : t.accent);
        c.drawCircle(px, py, 5, t.fg);

        // numbers
        c.setFont(&fonts::Font0);
        c.setTextSize(1);
        c.setTextDatum(textdatum_t::top_left);
        int tx = 124, ty = CONTENT_Y + 8;
        char buf[40];
        c.setTextColor(t.accent, t.bg);
        c.drawString("accel [g]", tx, ty);
        c.setTextColor(t.fg, t.bg);
        snprintf(buf, sizeof(buf), "x %+6.2f", _ax);
        c.drawString(buf, tx, ty + 12);
        snprintf(buf, sizeof(buf), "y %+6.2f", _ay);
        c.drawString(buf, tx, ty + 22);
        snprintf(buf, sizeof(buf), "z %+6.2f", _az);
        c.drawString(buf, tx, ty + 32);

        c.setTextColor(t.accent2, t.bg);
        c.drawString("gyro [dps]", tx, ty + 50);
        c.setTextColor(t.fg, t.bg);
        snprintf(buf, sizeof(buf), "x %+7.1f", _gx);
        c.drawString(buf, tx, ty + 62);
        snprintf(buf, sizeof(buf), "y %+7.1f", _gy);
        c.drawString(buf, tx, ty + 72);
        snprintf(buf, sizeof(buf), "z %+7.1f", _gz);
        c.drawString(buf, tx, ty + 82);

        ui::hintBar(c, t, "Esc: back");
    }

private:
    bool _enabled = false;
    float _ax = 0, _ay = 0, _az = 0, _gx = 0, _gy = 0, _gz = 0;
};

}  // namespace

App* createImuApp()
{
    return new ImuApp();
}
