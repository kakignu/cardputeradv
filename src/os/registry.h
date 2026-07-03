// ClaudeOS — application registry.
// Every launchable app registers a name, an icon painter and a factory here.
#pragma once
#include <M5Cardputer.h>
#include <vector>

#include "os.h"
#include "theme.h"

struct AppInfo {
    const char* id;    // short id used by the launcher / terminal ("files")
    const char* name;  // display name ("Files")
    void (*icon)(M5Canvas& c, int x, int y, const Theme& t);  // 24x24 icon
    App* (*create)();
};

const std::vector<AppInfo>& appRegistry();
App* createAppById(const String& id);
