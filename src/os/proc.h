// ClaudeOS — process manager (Option C hybrid multitasking).
//
// The UI stays single-threaded (the main loop / app stack), while background
// "jobs" run as real FreeRTOS tasks with their own stacks, preemptively
// scheduled across both cores. Jobs communicate with the UI only through
// thread-safe primitives: the job table (mutex-protected), progress fields,
// and the OS notification queue (JobCtx::post -> toast on screen).
//
// Rules for job code:
//   * never touch the canvas / display — post a notification instead
//   * poll ctx.cancelled() and exit promptly when asked
//   * filesystem access is safe: vfs takes a global FS mutex per operation
#pragma once
#include <Arduino.h>
#include <functional>

namespace proc {

constexpr int MAX_JOBS = 8;

struct JobCtx {
    int pid               = 0;
    volatile bool cancel  = false;
    volatile int progress = -1;  // -1 = indeterminate, else 0..100

    bool cancelled() const { return cancel; }
    void post(const String& text);  // thread-safe toast notification
};

using JobFn = std::function<void(JobCtx&)>;

struct Info {
    int pid;
    char name[20];
    int progress;
    uint32_t stackFree;  // minimum free stack ever observed (bytes)
    uint32_t runMs;
};

void begin();

// Start a background job. Returns pid, or -1 if the job table is full /
// task creation failed. core: 0 = share with WiFi stack, 1 = share with UI.
int spawn(const char* name, JobFn fn, uint32_t stackBytes = 4096,
          uint32_t priority = 1, int core = 0);

// Cooperative cancellation: sets ctx.cancel, the job exits on its own.
bool requestCancel(int pid);

// Force kill (vTaskDelete). Dangerous: a job holding a lock will deadlock
// later users of that lock. Exposed for education ("kill -9").
bool forceKill(int pid);

int list(Info* out, int max);
int runningCount();
bool isAlive(int pid);

}  // namespace proc
