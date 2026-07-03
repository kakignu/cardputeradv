#include "storage.h"

#include <LittleFS.h>
#include <SD.h>

#include <algorithm>
#include <cstring>

#include "os.h"

namespace vfs {

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
    if (isRoot(path)) return true;
    String sub;
    fs::FS* fs = resolve(path, sub);
    if (!fs) return false;
    if (sub == "/") return true;
    return fs->exists(sub);
}

bool isDir(const String& path)
{
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
    String sub;
    fs::FS* fs = resolve(path, sub);
    if (!fs || sub == "/") return false;
    if (isDir(path)) return fs->rmdir(sub);
    return fs->remove(sub);
}

bool makeDir(const String& path)
{
    String sub;
    fs::FS* fs = resolve(path, sub);
    if (!fs || sub == "/") return false;
    return fs->mkdir(sub);
}

bool touch(const String& path)
{
    String sub;
    fs::FS* fs = resolve(path, sub);
    if (!fs || sub == "/") return false;
    if (fs->exists(sub)) return true;
    File f = fs->open(sub, FILE_WRITE);
    if (!f) return false;
    f.close();
    return true;
}

}  // namespace vfs
