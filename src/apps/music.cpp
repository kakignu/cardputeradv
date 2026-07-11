// ClaudeOS Music — background playback demo for the process manager.
//
// The melody is played by a background job (FreeRTOS task, see os/proc.h),
// NOT by this app. Close the app, browse files, play Snake — the music
// keeps going, because the job has its own thread of execution.
#include "music.h"

#include "../os/proc.h"
#include "../os/widgets.h"

namespace {

struct Note {
    uint16_t freq;  // Hz, 0 = rest
    uint16_t ms;
};

// Korobeiniki (Russian folk tune) — public domain
const Note MELODY[] = {
    {659, 400}, {494, 200}, {523, 200}, {587, 400}, {523, 200}, {494, 200},
    {440, 400}, {440, 200}, {523, 200}, {659, 400}, {587, 200}, {523, 200},
    {494, 600}, {523, 200}, {587, 400}, {659, 400},
    {523, 400}, {440, 400}, {440, 400}, {0, 400},
    {587, 400}, {698, 200}, {880, 400}, {784, 200}, {698, 200},
    {659, 600}, {523, 200}, {659, 400}, {587, 200}, {523, 200},
    {494, 400}, {494, 200}, {523, 200}, {587, 400}, {659, 400},
    {523, 400}, {440, 400}, {440, 400}, {0, 600},
};

int s_pid = -1;  // pid of the playback job (UI thread only)

void startPlayback()
{
    s_pid = proc::spawn(
        "music",
        [](proc::JobCtx& ctx) {
            size_t i = 0;
            while (!ctx.cancelled()) {
                const Note& n = MELODY[i];
                // channel 1 = music, so UI clicks (channel 0) don't collide
                if (n.freq) M5Cardputer.Speaker.tone(n.freq, n.ms - 30, 1);
                ctx.progress = (int)(i * 100 / (sizeof(MELODY) / sizeof(MELODY[0])));
                vTaskDelay(pdMS_TO_TICKS(n.ms));
                i = (i + 1) % (sizeof(MELODY) / sizeof(MELODY[0]));
            }
            M5Cardputer.Speaker.stop(1);
        },
        4096, 1, 0);  // runs on core 0, next to the WiFi stack
}

class MusicApp : public App {
public:
    const char* title() const override { return "Music"; }

    void onKey(const KeyEvent& e) override
    {
        switch (e.key) {
            case Key::Enter:
                if (musicIsPlaying()) {
                    proc::requestCancel(s_pid);
                } else {
                    startPlayback();
                }
                break;
            case Key::Esc:
                OS::get().closeTop();  // playback continues!
                break;
            default:
                break;
        }
    }

    void onDraw(M5Canvas& c) override
    {
        const Theme& t = OS::get().theme();
        bool playing   = musicIsPlaying();

        c.setTextDatum(textdatum_t::middle_center);
        c.setFont(&fonts::Font0);

        // big note glyph (drawn, Font0 has no music glyphs)
        int cx = SCREEN_W / 2, cy = 52;
        uint16_t col = playing ? t.accent : t.dim;
        c.fillEllipse(cx - 10, cy + 14, 7, 5, col);
        c.fillRect(cx - 4, cy - 14, 3, 28, col);
        c.fillEllipse(cx + 14, cy + 10, 7, 5, col);
        c.fillRect(cx + 20, cy - 18, 3, 28, col);
        c.fillRect(cx - 4, cy - 18, 27, 4, col);
        if (playing) {
            // animated "sound waves"
            int k = (millis() / 200) % 3;
            for (int i = 0; i <= k; i++) {
                c.drawArc(cx + 40, cy, 10 + i * 7, 10 + i * 7, -60, 60, t.accent2);
            }
        }

        c.setTextSize(1);
        c.setTextColor(t.fg, t.bg);
        c.drawString("Korobeiniki (Tetris theme)", cx, 92);
        c.setTextColor(playing ? t.ok : t.dim, t.bg);
        c.drawString(playing ? "PLAYING in background (job on core 0)" : "stopped", cx, 106);

        ui::hintBar(c, t, playing ? "Enter: stop   Esc: close (music keeps playing!)"
                                  : "Enter: play   Esc: close");
    }
};

}  // namespace

bool musicIsPlaying()
{
    return s_pid >= 0 && proc::isAlive(s_pid);
}

App* createMusicApp()
{
    return new MusicApp();
}
