#pragma once

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <initializer_list>
#include <ui/monitor_icons.hpp>

// Board-independent drawing helpers. TFT_eSPI and the host preview share these.
namespace ui { namespace monitor {
constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xf8u) << 8) | ((g & 0xfcu) << 3) | (b >> 3);
}
constexpr uint16_t kBackground = rgb(0, 3, 6);
constexpr uint16_t kPanel = rgb(5, 15, 22);
constexpr uint16_t kTrack = rgb(17, 40, 50);
constexpr uint16_t kElapsed = rgb(112, 128, 138);
constexpr uint16_t kBorder = rgb(16, 78, 90);
constexpr uint16_t kCyan = rgb(17, 221, 249);
constexpr uint16_t kMint = rgb(42, 246, 162);
constexpr uint16_t kText = rgb(227, 246, 253);
constexpr uint16_t kMuted = rgb(126, 162, 178);
constexpr uint16_t kAmber = rgb(255, 195, 64);
constexpr uint16_t kRed = rgb(255, 99, 121);

inline int remaining(int used) { return used < 0 ? -1 : 100 - used; }

template<class Display>
void frame(Display& d, int x, int y, int w, int h, uint16_t accent) {
  d.fillRect(x+1, y+1, w-2, h-2, kPanel);
  constexpr int c = 5;
  d.drawFastHLine(x+c, y, w-2*c, accent);
  d.drawFastHLine(x+c, y+h-1, w-2*c, accent);
  d.drawFastVLine(x, y+c, h-2*c, accent);
  d.drawFastVLine(x+w-1, y+c, h-2*c, accent);
  d.drawLine(x, y+c, x+c, y, accent);
  d.drawLine(x+w-c-1, y, x+w-1, y+c, accent);
  d.drawLine(x, y+h-c-1, x+c, y+h-1, accent);
  d.drawLine(x+w-c-1, y+h-1, x+w-1, y+h-c-1, accent);
}

template<class Display>
void icon(Display& d, const uint8_t* mask, int w, int h, int x, int y, uint16_t tint) {
  // Reference glow/antialias coverage is blended against the panel color.
  const int tr = ((tint >> 11) & 31)*255/31, tg = ((tint >> 5) & 63)*255/63, tb = (tint & 31)*255/31;
  for (int row=0; row<h; ++row) for (int col=0; col<w; ++col) {
    const int alpha = mask[row*w+col];
    if (!alpha) continue;
    d.drawPixel(x+col, y+row, rgb((tr*alpha+5*(255-alpha))/255,
                                (tg*alpha+15*(255-alpha))/255,
                                (tb*alpha+22*(255-alpha))/255));
  }
}

template<class Display>
void segments(Display& d, int x, int y, int w, int h, int percent, uint16_t accent, int count=10) {
  d.drawRect(x, y, w, h, kBorder);
  for (int i=0; i<count; ++i) {
    const int left=x+2+(w-4)*i/count;
    const int right=x+2+(w-4)*(i+1)/count-1;
    const int width=right-left;
    d.fillRect(left, y+2, width, h-4, kTrack);
    if (percent < 0) continue;
    int amount=percent*count-i*100;
    if (amount<0) amount=0;
    if (amount>100) amount=100;
    const int filled=width*amount/100;
    if (filled>0) d.fillRect(left, y+2, filled, h-4, accent);
  }
}

// Reset countdowns use time, independently of the remaining usage percentage.
inline int resetPixels(int64_t resets, int64_t now, int64_t window, int width) {
  if (resets < 0 || now <= 0) return -1;
  if (resets <= now) return 0;
  const int64_t seconds = resets - now;
  if (seconds >= window) return width;
  return static_cast<int>(seconds * width / window);
}
constexpr int kFiveHourResetWidth = 16;
constexpr int kWeeklyResetWidth = 24;

template<class Display>
void resetBar(Display& d, int x, int width, int filled, uint16_t accent) {
  d.fillRect(x, 12, width, 5, filled < 0 ? kTrack : kElapsed);
  if (filled > 0) d.fillRect(x, 12, filled > width ? width : filled, 5, accent);
  if (filled < 0) d.drawFastHLine(x + width/2 - 1, 14, 3, kMuted);
}

template<class Display>
void statusBar(Display& d, bool wifi, int rssi, const char* integration,
               bool active, bool attention, bool error, int fiveHour, int weekly, int fiveHourReset = -1, int weeklyReset = -1, const char* microState = nullptr) {
  using namespace monitor_icons;
  frame(d, 4, 4, 312, 36, kCyan);
  for (int x : {80, 160, 232}) d.drawFastVLine(x, 11, 22, kBorder);
  icon(d, kWifi, kWifiWidth, kWifiHeight, 9, 13, wifi ? kCyan : kMuted);
  d.setTextColor(kText, kPanel); d.drawString("Wi-Fi", 35, 10, 1);
  d.setTextColor(wifi ? kMint : kRed, kPanel); d.drawString(wifi ? "Online" : "Offline", 35, 25, 1);
  const int strength=!wifi ? 0 : (rssi>=-55 ? 4 : (rssi>=-65 ? 3 : (rssi>=-75 ? 2 : 1)));
  for (int i=0; i<4; ++i) d.fillRect(67+i*3, 19-(2+i*2), 2, 2+i*2, i<strength ? kMint : kTrack);
  const bool connected=!strcmp(integration, "connected");
  const bool degraded=!strcmp(integration, "degraded");
  const uint16_t stateColor=microState ? (!strcmp(microState, "Linked") ? kMint : (!strcmp(microState, "Off") ? kRed : kAmber)) : !connected ? (degraded ? kAmber : kRed) :
                            (error ? kRed : (attention ? kAmber : (active ? kCyan : kMint)));
  icon(d, kRobot, kRobotWidth, kRobotHeight, 86, 9, stateColor);
  d.setTextColor(kText, kPanel); d.drawString(microState ? "Micro" : "Codex", 113, 10, 1);
  d.setTextColor(stateColor, kPanel);
  const char* state = microState ? microState : (!connected ? (degraded ? "Check" : "Offline") :
               (error ? "Error" : (attention ? "Input" : (active ? "Busy" : "Online"))));
  d.drawString(state, 113, 25, 1);
  char value[16];
  d.setTextColor(kCyan, kPanel); d.drawString("5H", 167, 10, 1);
  if (fiveHour<0) snprintf(value,sizeof(value),"--%%"); else snprintf(value,sizeof(value),"%d%%",fiveHour);
  d.drawRightString(value,225,10,1);
  resetBar(d,182,kFiveHourResetWidth,fiveHourReset,kCyan); segments(d,166,24,61,11,fiveHour,kCyan,8);
  d.setTextColor(kMint, kPanel); d.drawString("WK", 239, 10, 1);
  if (weekly<0) snprintf(value,sizeof(value),"--%%"); else snprintf(value,sizeof(value),"%d%%",weekly);
  d.drawRightString(value,309,10,1);
  resetBar(d,254,kWeeklyResetWidth,weeklyReset,kMint); segments(d,238,24,71,11,weekly,kMint,8);
}

template<class Display>
void quotaCard(Display& d, int x, const char* title, int percent, bool weekly) {
  using namespace monitor_icons;
  const uint16_t accent=weekly ? kMint : kCyan;
  frame(d,x,60,150,122,accent);
  icon(d,weekly ? kCalendar : kClock,16,16,x+11,70,accent);
  d.setTextColor(kText,kPanel);d.drawString(title,x+34,70,2);
  d.drawFastHLine(x+10,92,130,kBorder);
  char value[16]; if(percent<0) snprintf(value,sizeof(value),"--%%"); else snprintf(value,sizeof(value),"%d%%",percent);
  d.setTextColor(accent,kPanel);d.drawCentreString(value,x+75,100,4);
  d.setTextColor(kMuted,kPanel);d.drawCentreString("remaining",x+75,133,1);
  segments(d,x+11,153,128,14,percent,accent);
}

template<class Display>
void commandHelp(Display& d, const char* const* phrases, size_t count) {
  frame(d,8,59,304,123,kAmber);
  d.setTextColor(kAmber,kPanel); d.drawString("SAY A DEVICE COMMAND",18,66,1);
  d.setTextColor(kText,kPanel);
  for (size_t i=0; i<count && i<5; ++i) d.drawString(phrases[i],18,80+i*17,2);
  d.setTextColor(kMuted,kPanel); d.drawString("Or tap below for a Codex message",18,171,1);
}

template<class Display>
void messagePanel(Display& d, bool recording, bool preparing, bool selected) {
  frame(d,8,59,304,123,recording ? kRed : kCyan);
  d.setTextColor(kText,kPanel);
  d.drawString(selected ? "Message to selected agent" : "Message to new Codex chat",18,70,2);
  d.setTextColor(kMuted,kPanel);
  if (recording) {
    d.drawString("Take your time. Pauses are OK.",18,104,1);
    d.drawString("Stops after ~5 seconds of silence",18,122,1);
    d.drawString("or 30 seconds. Tap below to send.",18,140,1);
  } else d.drawString(preparing ? "Preparing microphone..." : "Sending for transcription...",18,104,1);
}

template<class Display>
void voiceMeter(Display& d, bool metering, int level, uint16_t accent) {
  d.fillRect(276,198,28,28,kPanel);
  const int lengths[7]={4,9,16,25,16,9,4};
  const int volume = level < 0 ? 0 : (level > 100 ? 100 : level);
  for(int i=0;i<7;++i) {
    const int height = metering ? 2 + lengths[i] * volume / 100 : 2;
    d.drawFastVLine(278+i*4,212-height/2,height,metering ? accent : kMuted);
  }
}

template<class Display>
void voiceControl(Display& d, bool recording, bool preparing, bool busy, bool selected,
                  bool commands = false, int level = 0, bool ready = true, bool wifiOnly = false) {
  using namespace monitor_icons;
  const uint16_t accent=recording ? kRed : (commands ? kAmber : (busy ? kCyan : (ready ? kMint : kMuted)));
  frame(d,4,191,312,46, recording || commands ? accent : kBorder);
  d.drawCircle(31,214,19,kBorder);d.drawCircle(31,214,17,accent);
  icon(d,kMicrophone,kMicrophoneWidth,kMicrophoneHeight,22,200,accent);
  const char* label=recording ? "Recording message" : (preparing ? "Preparing mic" :
                    (busy ? "Sending voice" : (commands ? "Command listening" :
                    (!ready ? "Mic unavailable" : (selected ? "Message to agent" : (wifiOnly ? "Wi-Fi voice" : "New Codex message"))))));
  d.setTextColor(kText,kPanel);d.drawString(label,61,199,2);
  d.setTextColor(accent,kPanel);d.drawString(recording ? "Tap to send / pause to finish" : (preparing ? "Please wait" : (busy ? "Transcribing..." : (commands ? "Say a phrase / tap to talk" : (ready ? "Tap to talk / Hi ESP: commands" : "Speech starting or unavailable")))),62,222,1);
  voiceMeter(d, recording || commands, level, accent);
}
// Show both voice paths only while Desktop Micro has been detected.
template<class Display>
void dualVoiceControl(Display& d, bool microLinked, bool micReady, bool micHeld,
                      bool usbStreaming, int micLevel, bool wifiReady, bool recording,
                      bool preparing, bool busy, bool commands, int wifiLevel) {
  frame(d,4,191,152,46,micHeld ? kRed : kBorder);
  frame(d,160,191,156,46,recording ? kRed : kBorder);
  d.setTextColor(kText,kPanel); d.drawString("Micro voice",12,199,1); d.drawString("Wi-Fi voice",168,199,1);
  d.setTextColor(microLinked && micReady ? kMint : kMuted,kPanel);
  d.drawString(!micReady ? "Mic unavailable" : (!microLinked ? "USB / no app" : (micHeld ? "Mic key held" : "Tap voice / hold PTT")),12,222,1);
  d.setTextColor(wifiReady ? kMint : kMuted,kPanel);
  d.drawString(recording ? "Recording / tap send" : (preparing ? "Preparing..." : (busy ? "Sending..." :
    (commands ? "Say command" : (wifiReady ? "Tap to talk" : "Speech unavailable")))),168,222,1);
  const int heights[7]={4,9,16,25,16,9,4};
  for (int i=0;i<7;++i) {
    const int a = (micHeld || usbStreaming) ? 2 + heights[i] * micLevel / 100 : 2;
    const int b = (recording || commands) ? 2 + heights[i] * wifiLevel / 100 : 2;
    d.drawFastVLine(124+i*4,212-a/2,a,kCyan); d.drawFastVLine(280+i*4,212-b/2,b,kMint);
  }
}
} }  // namespace ui::monitor
