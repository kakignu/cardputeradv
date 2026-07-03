#pragma once
#include <Arduino.h>

#include "../os/os.h"

// path may be empty for a new untitled note.
App* createEditorApp(const String& path = "");
