#pragma once
#if defined(FNK0104B_ENABLE_MONITOR_PHOTOS)
#include <FS.h>
#include <stdint.h>
#include <stddef.h>
namespace fnk0104b::monitor_photos {
bool mount();
bool scan(char names[8][25], unsigned& count);
bool beginUpload(const char* name);
bool write(const uint8_t* data, size_t size);
bool commit(const char* name);
void abort();
bool remove(const char* name);
fs::File open(const char* name);
uint16_t* allocateFrame();
void releaseFrame(uint16_t* pixels);
void drawFrame(uint16_t* pixels);
}
#endif
