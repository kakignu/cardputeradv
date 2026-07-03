// ClaudeOS — an original little operating system for the M5Stack Cardputer ADV,
// designed and written by Claude.
//
// Entry point: everything interesting lives in src/os/ (kernel, UI, services)
// and src/apps/ (the built-in applications).
#include "os/os.h"

void setup()
{
    OS::get().begin();
}

void loop()
{
    OS::get().tick();
}
