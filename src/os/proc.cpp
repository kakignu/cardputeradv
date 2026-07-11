#include "proc.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <cstring>

#include "os.h"

namespace proc {

struct Slot {
    bool used           = false;
    int pid             = 0;
    char name[20]       = {0};
    JobFn fn;
    TaskHandle_t handle = nullptr;
    JobCtx ctx;
    uint32_t startedMs  = 0;
};

static Slot s_slots[MAX_JOBS];
static SemaphoreHandle_t s_mtx = nullptr;
static int s_nextPid           = 1;

void JobCtx::post(const String& text)
{
    OS::get().postNotify(text.c_str());
}

void begin()
{
    if (!s_mtx) s_mtx = xSemaphoreCreateMutex();
}

static void lock()
{
    xSemaphoreTake(s_mtx, portMAX_DELAY);
}
static void unlock()
{
    xSemaphoreGive(s_mtx);
}

// Every job runs inside this wrapper. When the job function returns, the
// slot is freed and the task deletes itself.
static void trampoline(void* p)
{
    Slot* s = static_cast<Slot*>(p);
    s->fn(s->ctx);

    lock();
    s->fn     = nullptr;  // destroy captured state on the job's own thread
    s->handle = nullptr;
    s->used   = false;
    unlock();
    vTaskDelete(nullptr);
}

int spawn(const char* name, JobFn fn, uint32_t stackBytes, uint32_t priority, int core)
{
    if (!s_mtx || !fn) return -1;
    lock();
    Slot* slot = nullptr;
    for (auto& s : s_slots) {
        if (!s.used) {
            slot = &s;
            break;
        }
    }
    if (!slot) {
        unlock();
        return -1;
    }
    slot->used = true;
    slot->pid  = s_nextPid++;
    strncpy(slot->name, name, sizeof(slot->name) - 1);
    slot->name[sizeof(slot->name) - 1] = 0;
    slot->fn           = std::move(fn);
    slot->ctx.pid      = slot->pid;
    slot->ctx.cancel   = false;
    slot->ctx.progress = -1;
    slot->startedMs    = millis();

    BaseType_t ok = xTaskCreatePinnedToCore(trampoline, slot->name, stackBytes, slot,
                                            priority, &slot->handle, core);
    if (ok != pdPASS) {
        slot->fn   = nullptr;
        slot->used = false;
        unlock();
        return -1;
    }
    int pid = slot->pid;
    unlock();
    return pid;
}

bool requestCancel(int pid)
{
    if (!s_mtx) return false;
    lock();
    for (auto& s : s_slots) {
        if (s.used && s.pid == pid) {
            s.ctx.cancel = true;
            unlock();
            return true;
        }
    }
    unlock();
    return false;
}

bool forceKill(int pid)
{
    if (!s_mtx) return false;
    lock();
    for (auto& s : s_slots) {
        if (s.used && s.pid == pid) {
            if (s.handle) vTaskDelete(s.handle);
            s.fn     = nullptr;
            s.handle = nullptr;
            s.used   = false;
            unlock();
            return true;
        }
    }
    unlock();
    return false;
}

int list(Info* out, int max)
{
    if (!s_mtx) return 0;
    int n = 0;
    lock();
    for (auto& s : s_slots) {
        if (!s.used || n >= max) continue;
        Info& i = out[n++];
        i.pid   = s.pid;
        strncpy(i.name, s.name, sizeof(i.name));
        i.progress  = s.ctx.progress;
        i.stackFree = s.handle ? uxTaskGetStackHighWaterMark(s.handle) : 0;
        i.runMs     = millis() - s.startedMs;
    }
    unlock();
    return n;
}

int runningCount()
{
    if (!s_mtx) return 0;
    int n = 0;
    lock();
    for (auto& s : s_slots)
        if (s.used) n++;
    unlock();
    return n;
}

bool isAlive(int pid)
{
    if (!s_mtx) return false;
    lock();
    for (auto& s : s_slots) {
        if (s.used && s.pid == pid) {
            unlock();
            return true;
        }
    }
    unlock();
    return false;
}

}  // namespace proc
