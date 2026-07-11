// ClaudeOS Tasks — process monitor.
//
// Two views (Tab to switch):
//   Jobs   — ClaudeOS background jobs (proc.h): progress, stack, cancel/kill
//   System — every FreeRTOS task on the chip (WiFi stack, IDLE, our UI loop
//            ...), which shows that a real preemptive kernel runs underneath.
#include "tasks_app.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "../os/proc.h"
#include "../os/widgets.h"

namespace {

class TasksApp : public App {
public:
    const char* title() const override { return _sysView ? "Tasks: system" : "Tasks: jobs"; }

    void onKey(const KeyEvent& e) override
    {
        switch (e.key) {
            case Key::Tab:
                _sysView = !_sysView;
                _sel     = 0;
                break;
            case Key::Up:
                _sel--;
                break;
            case Key::Down:
                _sel++;
                break;
            case Key::Char:
                if (!_sysView && e.ch == 'k') {  // cooperative cancel
                    if (_sel < _jobCount) {
                        proc::requestCancel(_jobs[_sel].pid);
                        OS::get().beep(660, 40);
                    }
                } else if (!_sysView && e.ch == 'x') {  // force kill
                    if (_sel < _jobCount) {
                        proc::forceKill(_jobs[_sel].pid);
                        OS::get().beep(220, 80);
                    }
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
        if (millis() - _lastPoll < 250) return;  // refresh 4x/sec
        _lastPoll = millis();
        _jobCount = proc::list(_jobs, proc::MAX_JOBS);
#if configUSE_TRACE_FACILITY == 1
        _sysCount = uxTaskGetSystemState(_sys, MAX_SYS, nullptr);
#endif
    }

    void onDraw(M5Canvas& c) override
    {
        const Theme& t = OS::get().theme();
        c.setFont(&fonts::Font0);
        c.setTextSize(1);
        c.setTextDatum(textdatum_t::top_left);

        // header: UI load + heap
        char hdr[64];
        snprintf(hdr, sizeof(hdr), "UI load %2d%%   heap %uK free   jobs %d",
                 (int)(OS::get().uiLoad() * 100), (unsigned)(ESP.getFreeHeap() / 1024),
                 proc::runningCount());
        c.setTextColor(t.accent, t.bg);
        c.drawString(hdr, 4, CONTENT_Y + 1);
        // UI load bar
        int bw = (int)(232 * OS::get().uiLoad());
        c.fillRoundRect(4, CONTENT_Y + 11, 232, 3, 1, t.panel);
        c.fillRoundRect(4, CONTENT_Y + 11, bw, 3, 1,
                        OS::get().uiLoad() > 0.9f ? t.danger : t.accent2);

        int y = CONTENT_Y + 19;
        if (_sysView) {
            drawSystem(c, t, y);
        } else {
            drawJobs(c, t, y);
        }

        ui::hintBar(c, t, _sysView ? "Tab: jobs view   Esc: back"
                                   : "K: cancel  X: force kill  Tab: system  Esc: back");
    }

private:
    proc::Info _jobs[proc::MAX_JOBS];
    int _jobCount = 0;
#if configUSE_TRACE_FACILITY == 1
    static constexpr int MAX_SYS = 24;
    TaskStatus_t _sys[MAX_SYS];
    int _sysCount = 0;
#else
    int _sysCount = 0;
#endif
    int _sel          = 0;
    bool _sysView     = false;
    uint32_t _lastPoll = 0;

    void drawJobs(M5Canvas& c, const Theme& t, int y)
    {
        if (_jobCount == 0) {
            c.setTextColor(t.dim, t.bg);
            c.drawString("No background jobs.", 6, y + 8);
            c.drawString("Try:  Term> worker 20 &", 6, y + 22);
            c.drawString("      Term> copy /sd/a.bin /flash/a.bin &", 6, y + 34);
            c.drawString("      Music app Enter, then Esc", 6, y + 46);
            return;
        }
        if (_sel >= _jobCount) _sel = _jobCount - 1;
        if (_sel < 0) _sel = 0;

        c.setTextColor(t.dim, t.bg);
        c.drawString("PID NAME          PROGRESS      STACK  TIME", 4, y);
        y += 11;
        for (int i = 0; i < _jobCount; i++) {
            bool sel = (i == _sel);
            if (sel) c.fillRoundRect(2, y - 1, 236, 12, 2, t.sel);
            uint16_t fg = sel ? t.selText : t.fg;
            uint16_t bg = sel ? t.sel : t.bg;
            char ln[64];
            snprintf(ln, sizeof(ln), "%3d %-13s", _jobs[i].pid, _jobs[i].name);
            c.setTextColor(fg, bg);
            c.drawString(ln, 4, y + 1);
            // progress bar or spinner
            int px = 112, pw = 76;
            if (_jobs[i].progress >= 0) {
                c.drawRect(px, y + 1, pw, 8, sel ? t.selText : t.dim);
                c.fillRect(px + 1, y + 2, (pw - 2) * _jobs[i].progress / 100, 6, t.ok);
            } else {
                int k = (millis() / 120) % pw;
                c.drawRect(px, y + 1, pw, 8, sel ? t.selText : t.dim);
                c.fillRect(px + 1 + k * (pw - 14) / pw, y + 2, 12, 6, t.accent2);
            }
            snprintf(ln, sizeof(ln), "%4uB %3us", (unsigned)_jobs[i].stackFree,
                     (unsigned)(_jobs[i].runMs / 1000));
            c.setTextColor(sel ? t.selText : t.dim, bg);
            c.drawString(ln, 192, y + 1);
            y += 13;
        }
    }

    void drawSystem(M5Canvas& c, const Theme& t, int y)
    {
#if configUSE_TRACE_FACILITY == 1
        c.setTextColor(t.dim, t.bg);
        c.drawString("NAME          ST PRI  STACK-FREE  (FreeRTOS)", 4, y);
        y += 11;
        int rows = (SCREEN_H - y - 12) / 10;
        for (int i = 0; i < _sysCount && i < rows; i++) {
            const TaskStatus_t& s = _sys[i];
            const char* st        = "?";
            switch (s.eCurrentState) {
                case eRunning:   st = "R"; break;
                case eReady:     st = "r"; break;
                case eBlocked:   st = "B"; break;
                case eSuspended: st = "S"; break;
                case eDeleted:   st = "D"; break;
                default: break;
            }
            char ln[64];
            snprintf(ln, sizeof(ln), "%-13s %s %3u  %6uB", s.pcTaskName, st,
                     (unsigned)s.uxCurrentPriority, (unsigned)s.usStackHighWaterMark);
            bool ours = strcmp(s.pcTaskName, "loopTask") == 0;
            c.setTextColor(ours ? t.accent : t.fg, t.bg);
            c.drawString(ln, 4, y);
            y += 10;
        }
#else
        // Arduino builds ship FreeRTOS without the trace facility, so a full
        // per-task listing isn't available — show what we can measure.
        char ln[64];
        c.setTextColor(t.fg, t.bg);
        snprintf(ln, sizeof(ln), "FreeRTOS tasks alive: %u",
                 (unsigned)uxTaskGetNumberOfTasks());
        c.drawString(ln, 6, y + 2);
        snprintf(ln, sizeof(ln), "UI task 'loopTask': core %d, prio %u",
                 xPortGetCoreID(), (unsigned)uxTaskPriorityGet(nullptr));
        c.drawString(ln, 6, y + 16);
        snprintf(ln, sizeof(ln), "  stack min free: %uB",
                 (unsigned)uxTaskGetStackHighWaterMark(nullptr));
        c.drawString(ln, 6, y + 28);
        c.setTextColor(t.dim, t.bg);
        c.drawString("(WiFi/BT/IDLE tasks run alongside; a full", 6, y + 46);
        c.drawString(" listing needs an ESP-IDF build with", 6, y + 56);
        c.drawString(" CONFIG_FREERTOS_USE_TRACE_FACILITY=y)", 6, y + 66);
#endif
    }
};

}  // namespace

App* createTasksApp()
{
    return new TasksApp();
}
