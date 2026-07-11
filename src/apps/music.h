#pragma once
#include "../os/os.h"

App* createMusicApp();

// true while the background playback job is alive (used by the status bar
// and by the app itself — playback survives closing the app).
bool musicIsPlaying();
