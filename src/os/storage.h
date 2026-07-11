// ClaudeOS — unified virtual file system.
//
// Paths:  /sd/...     microSD card   (SPI: SCK=G40 MISO=G39 MOSI=G14 CS=G12)
//         /flash/...  internal LittleFS partition
// "/" is a virtual root listing the mounted volumes.
#pragma once
#include <Arduino.h>
#include <FS.h>
#include <functional>
#include <vector>

namespace vfs {

// Create the global FS mutex. Call once from OS::begin() before any job
// can run. Every vfs operation below then serializes on this mutex, which
// makes the whole VFS safe to use from background jobs (proc.h).
void initLocks();

struct Entry {
    String name;
    bool dir    = false;
    size_t size = 0;
};

// Resolve "/sd/foo" -> (&SD, "/foo"). Returns nullptr for "/" or unknown roots.
fs::FS* resolve(const String& path, String& sub);

bool isRoot(const String& path);
String parent(const String& path);
String join(const String& dir, const String& name);

bool list(const String& path, std::vector<Entry>& out);
bool exists(const String& path);
bool isDir(const String& path);
size_t fileSize(const String& path);
bool readText(const String& path, String& out, size_t maxBytes = 30 * 1024);
bool writeText(const String& path, const String& data);
bool removePath(const String& path);  // file or empty dir
bool makeDir(const String& path);
bool touch(const String& path);

// Streaming copy in 4KB chunks. The FS mutex is taken per chunk (not for
// the whole copy), so the UI thread can interleave its own file access.
// progress(percent) is called between chunks; return false to abort.
bool copyFile(const String& src, const String& dst,
              const std::function<bool(int)>& progress = nullptr);

}  // namespace vfs
