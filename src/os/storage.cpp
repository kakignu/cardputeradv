#include "storage.h"

#include <LittleFS.h>
#include <SD.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <algorithm>
#include <cstring>
#include <memory>

#include "os.h"

namespace vfs {

// Global FS mutex: the SD/LittleFS drivers must not be entered concurrently
// from the UI thread and background jobs. Recursive so vfs functions can
// call each other (e.g. removePath -> isDir).
static SemaphoreHandle_t s_fsMtx = nullptr;

void initLocks()
{
    if (!s_fsMtx) s_fsMtx = xSemaphoreCreateRecursiveMutex();
}

struct FsLock {
    FsLock()
    {
        if (s_fsMtx) xSemaphoreTakeRecursive(s_fsMtx, portMAX_DELAY);
    }
    ~FsLock()
    {
        if (s_fsMtx) xSemaphoreGiveRecursive(s_fsMtx);
    }
};

fs::FS* resolve(const String& path, String& sub)
{
    if (path.startsWith("/sd")) {
        if (!OS::get().sdOk()) return nullptr;
        sub = path.substring(3);
        if (sub.isEmpty()) sub = "/";
        return &SD;
    }
    if (path.startsWith("/flash")) {
        if (!OS::get().flashOk()) return nullptr;
        sub = path.substring(6);
        if (sub.isEmpty()) sub = "/";
        return &LittleFS;
    }
    return nullptr;
}

bool isRoot(const String& path)
{
    return path == "/" || path.isEmpty();
}

String parent(const String& path)
{
    int idx = path.lastIndexOf('/');
    if (idx <= 0) return "/";
    return path.substring(0, idx);
}

String join(const String& dir, const String& name)
{
    if (dir.endsWith("/")) return dir + name;
    return dir + "/" + name;
}

bool list(const String& path, std::vector<Entry>& out)
{
    FsLock lk;
    out.clear();
    if (isRoot(path)) {
        if (OS::get().sdOk()) out.push_back({"sd", true, 0});
        if (OS::get().flashOk()) out.push_back({"flash", true, 0});
        return true;
    }
    String sub;
    fs::FS* fs = resolve(path, sub);
    if (!fs) return false;
    File dir = fs->open(sub);
    if (!dir || !dir.isDirectory()) return false;

    File f = dir.openNextFile();
    while (f) {
        Entry e;
        String n = f.name();
        int idx  = n.lastIndexOf('/');
        e.name   = (idx >= 0) ? n.substring(idx + 1) : n;
        e.dir    = f.isDirectory();
        e.size   = f.size();
        if (!e.name.isEmpty()) out.push_back(e);
        f = dir.openNextFile();
    }
    dir.close();
    // directories first, then alphabetical
    std::sort(out.begin(), out.end(), [](const Entry& a, const Entry& b) {
        if (a.dir != b.dir) return a.dir;
        return strcasecmp(a.name.c_str(), b.name.c_str()) < 0;
    });
    return true;
}

bool exists(const String& path)
{
    FsLock lk;
    if (isRoot(path)) return true;
    String sub;
    fs::FS* fs = resolve(path, sub);
    if (!fs) return false;
    if (sub == "/") return true;
    return fs->exists(sub);
}

bool isDir(const String& path)
{
    FsLock lk;
    if (isRoot(path)) return true;
    String sub;
    fs::FS* fs = resolve(path, sub);
    if (!fs) return false;
    if (sub == "/") return true;
    File f = fs->open(sub);
    bool d = f && f.isDirectory();
    if (f) f.close();
    return d;
}

size_t fileSize(const String& path)
{
    FsLock lk;
    String sub;
    fs::FS* fs = resolve(path, sub);
    if (!fs) return 0;
    File f = fs->open(sub, FILE_READ);
    if (!f) return 0;
    size_t s = f.size();
    f.close();
    return s;
}

bool readText(const String& path, String& out, size_t maxBytes)
{
    FsLock lk;
    out = "";
    String sub;
    fs::FS* fs = resolve(path, sub);
    if (!fs) return false;
    File f = fs->open(sub, FILE_READ);
    if (!f || f.isDirectory()) return false;
    size_t n = f.size();
    if (n > maxBytes) n = maxBytes;
    out.reserve(n + 1);
    while (n-- && f.available()) {
        char c = (char)f.read();
        if (c != '\r') out += c;
    }
    f.close();
    return true;
}

bool writeText(const String& path, const String& data)
{
    FsLock lk;
    String sub;
    fs::FS* fs = resolve(path, sub);
    if (!fs) return false;
    File f = fs->open(sub, FILE_WRITE);
    if (!f) return false;
    size_t written = f.print(data);
    f.close();
    return written == data.length();
}

bool removePath(const String& path)
{
    FsLock lk;
    String sub;
    fs::FS* fs = resolve(path, sub);
    if (!fs || sub == "/") return false;
    if (isDir(path)) return fs->rmdir(sub);
    return fs->remove(sub);
}

bool makeDir(const String& path)
{
    FsLock lk;
    String sub;
    fs::FS* fs = resolve(path, sub);
    if (!fs || sub == "/") return false;
    return fs->mkdir(sub);
}

bool touch(const String& path)
{
    FsLock lk;
    String sub;
    fs::FS* fs = resolve(path, sub);
    if (!fs || sub == "/") return false;
    if (fs->exists(sub)) return true;
    File f = fs->open(sub, FILE_WRITE);
    if (!f) return false;
    f.close();
    return true;
}

bool copyFile(const String& src, const String& dst, const std::function<bool(int)>& progress)
{
    String ssub, dsub;
    fs::FS* sfs = resolve(src, ssub);
    fs::FS* dfs = resolve(dst, dsub);
    if (!sfs || !dfs) return false;

    File in, out;
    size_t total = 0;
    {
        FsLock lk;
        in = sfs->open(ssub, FILE_READ);
        if (!in || in.isDirectory()) return false;
        total = in.size();
        out   = dfs->open(dsub, FILE_WRITE);
        if (!out) {
            in.close();
            return false;
        }
    }

    static constexpr size_t CHUNK = 4096;
    std::unique_ptr<uint8_t[]> buf(new uint8_t[CHUNK]);
    size_t done = 0;
    bool ok     = true;

    while (true) {
        size_t n;
        {
            // lock per chunk so the UI thread can interleave its own FS work
            FsLock lk;
            n = in.read(buf.get(), CHUNK);
            if (n == 0) break;
            if (out.write(buf.get(), n) != n) {
                ok = false;
                break;
            }
        }
        done += n;
        if (progress) {
            int pct = total ? (int)((uint64_t)done * 100 / total) : -1;
            if (!progress(pct)) {
                ok = false;
                break;
            }
        }
        vTaskDelay(1);  // yield between chunks
    }

    {
        FsLock lk;
        in.close();
        out.close();
        if (!ok) dfs->remove(dsub);  // don't leave partial files behind
    }
    return ok;
}

}  // namespace vfs
