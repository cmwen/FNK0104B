#pragma once
#include <BLEServer.h>
#include <Preferences.h>
#include <ui/idle_photos.hpp>
namespace monitor_photos {
void begin(Preferences& preferences);
void attach(BLEService* service);
void disconnected();
void tick(bool allowed);
ui::photos::Settings settings();
bool uploading();
bool changed();
void setIdle(bool idle);
bool draw();
bool preview();
void cancelPreview();
void diagnostic(const char* command);
}
