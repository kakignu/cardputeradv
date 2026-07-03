// ClaudeOS — unified virtual file system.
//
// Paths:  /sd/...     microSD card   (SPI: SCK=G40 MISO=G39 MOSI=G14 CS=G12)
//         /flash/...  internal LittleFS partition
// "/" is a virtual root listing the mounted volumes.
#pragma once
#include <Arduino.h>
#include <FS.h>
#include <vector>

namespace vfs {

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

}  // namespace vfs
